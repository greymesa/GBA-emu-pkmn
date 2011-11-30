#include "stream.h"
#include <cell/audio.h>
#include <cell/sysmodule.h>
#include <sys/synchronization.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define port_channels 2
#define port_channels_shift 1
#define samples_times_two 512

/*
////////////////////////////////
buffer_.h
*//////////////////////////////

#include "buffer_.h"

/*
////////////////////////////////
audioport.c
*//////////////////////////////


static void init_audioport(void)
{
   static int init_count = 0;
   if (init_count == 0)
   {
      cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO);
      cellAudioInit();
      init_count++;
   }
}

typedef struct audioport
{
   volatile uint64_t quit_thread;
   uint32_t audio_port;

   uint32_t channels;

   sys_lwmutex_t lock;
   pthread_mutex_t cond_lock;
   pthread_cond_t cond;
   pthread_t thread;

   fifo_buffer_t *buffer;

   cell_audio_sample_cb_t sample_cb;
   void *userdata;

   uint32_t is_paused;
} audioport_t;


static void* event_loop(void *data)
{
   audioport_t *port = data;

   sys_event_queue_t id;
   sys_ipc_key_t key;

   cellAudioCreateNotifyEventQueue(&id, &key);
   cellAudioSetNotifyEventQueue(key);

   //pull_event_loop - BEGIN
   sys_event_t event;

   int16_t *in_buf = memalign(128, samples_times_two * sizeof(int16_t));
   float *conv_buf = memalign(128, samples_times_two * sizeof(float));
   do
   {
         uint32_t has_read = samples_times_two;
         sys_lwmutex_lock(&port->lock, SYS_NO_TIMEOUT);
         uint32_t avail = fifo_read_avail(port->buffer);
         if (avail < samples_times_two * sizeof(int16_t))
            has_read = avail / sizeof(int16_t);

         fifo_read(port->buffer, in_buf, has_read * sizeof(int16_t));
         sys_lwmutex_unlock(&port->lock);

      uint32_t i = 0;

      if (has_read < samples_times_two)
         memset(in_buf + has_read, 0, (samples_times_two - has_read) * sizeof(int16_t));

      do{
         conv_buf[i] = (float)in_buf[i]/0x8000;
         i++;
      }while(i < samples_times_two);

      sys_event_queue_receive(id, &event, SYS_NO_TIMEOUT);
      cellAudioAddData(port->audio_port, conv_buf, CELL_AUDIO_BLOCK_SAMPLES, 1.0);

      pthread_cond_signal(&port->cond);
   }while(!port->quit_thread);
   free(conv_buf);
   //pull_event_loop - END

   cellAudioRemoveNotifyEventQueue(key);
   pthread_exit(NULL);
   return NULL;
}

static cell_audio_handle_t audioport_init(const struct cell_audio_params *params)
{
   init_audioport();

   audioport_t *handle = calloc(1, sizeof(*handle));

   CellAudioPortParam port_params;
   if(params->headset)
   {
      port_params.nChannel = params->channels;
      port_params.nBlock = 8;
      port_params.attr = CELL_AUDIO_PORTATTR_OUT_SECONDARY;
   }
   else
   {
      port_params.nChannel = params->channels;
      port_params.nBlock = 8;
      port_params.attr = 0;
   }

   handle->channels = params->channels;

   handle->sample_cb = params->sample_cb;
   handle->userdata = params->userdata;
   handle->buffer = fifo_new(params->buffer_size ? params->buffer_size : 4096);

   sys_lwmutex_attribute_t attr;
   sys_lwmutex_attribute_initialize(attr);
   sys_lwmutex_create(&handle->lock, &attr);
   pthread_mutex_init(&handle->cond_lock, NULL);
   pthread_cond_init(&handle->cond, NULL);

   cellAudioPortOpen(&port_params, &handle->audio_port);
   cellAudioPortStart(handle->audio_port);

   pthread_create(&handle->thread, NULL, event_loop, handle);
   return handle;
}

static void audioport_pause(cell_audio_handle_t handle)
{
   audioport_t *port = handle;
   port->is_paused = 1;
   cellAudioPortStop(port->audio_port);
}

static int32_t audioport_unpause(cell_audio_handle_t handle)
{
   audioport_t *port = handle;
   port->is_paused = 0;
   cellAudioPortStart(port->audio_port);
   return 0;
}

static uint32_t audioport_is_paused(cell_audio_handle_t handle)
{
   audioport_t *port = handle;
   return port->is_paused;
}

static void audioport_free(cell_audio_handle_t handle)
{
   audioport_t *port = handle;

   port->quit_thread = 1;
   pthread_join(port->thread, NULL);

   sys_lwmutex_destroy(&port->lock);
   pthread_mutex_destroy(&port->cond_lock);
   pthread_cond_destroy(&port->cond);

   if (port->buffer)
      fifo_free(port->buffer);

   cellAudioPortStop(port->audio_port);
   cellAudioPortClose(port->audio_port);

   free(port);
}

static uint32_t audioport_write_avail(cell_audio_handle_t handle)
{
   audioport_t *port = handle;

   sys_lwmutex_lock(&port->lock, SYS_NO_TIMEOUT);
   uint32_t ret = fifo_write_avail(port->buffer);
   sys_lwmutex_unlock(&port->lock);
   return ret / sizeof(int16_t);
}

static int32_t audioport_write(cell_audio_handle_t handle, const int16_t *data, uint32_t samples)
{
   uint32_t bytes = samples * sizeof(int16_t);

   audioport_t *port = handle;
   do
   {
      sys_lwmutex_lock(&port->lock, SYS_NO_TIMEOUT);
      uint32_t avail = fifo_write_avail(port->buffer);
      sys_lwmutex_unlock(&port->lock);

      uint32_t to_write = avail < bytes ? avail : bytes;
      if (to_write > 0)
      {
         sys_lwmutex_lock(&port->lock, SYS_NO_TIMEOUT);
         fifo_write(port->buffer, data, to_write);
         sys_lwmutex_unlock(&port->lock);
         bytes -= to_write;
         data += to_write >> 1;
      }
      else
      {
         pthread_mutex_lock(&port->cond_lock);
         pthread_cond_wait(&port->cond, &port->cond_lock);
         pthread_mutex_unlock(&port->cond_lock);
      }
   }while (bytes > 0);

   return 256;
}

static uint32_t audioport_alive(cell_audio_handle_t handle)
{
   (void)handle;
   return 1;
}

const cell_audio_driver_t cell_audio_audioport = {
   .init = audioport_init,
   .write = audioport_write,
   .write_avail = audioport_write_avail,
   .pause = audioport_pause,
   .unpause = audioport_unpause,
   .is_paused = audioport_is_paused,
   .alive = audioport_alive,
   .free = audioport_free
};
