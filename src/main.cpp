#include <stdio.h>
#include <emscripten.h>
#include <stdlib.h>
#include "libretro.h"
#include <string.h>

struct retro_game_info gameInfo;

extern uint8_t libretro_save_buf[0x20000 + 0x2000];

void myLogCb(enum retro_log_level level, const char *fmt, ...)
{

}

void myVideoCb(const void *data, unsigned width, unsigned height, size_t pitch)
{
    static uint8_t rgbaBuffer[240 * 160 * 4];
    uint8_t *ptr = rgbaBuffer;
    uint16_t *srcPtr = (uint16_t *)data;
    unsigned padding = (pitch / 2) - width;

    for (unsigned y = 0; y < 160; y++)
    {
        for (unsigned x = 0; x < 240; x++)
        {
            uint16_t c = *srcPtr;
            uint8_t r = ((c & 0xF800) >> 11) << 3;
            uint8_t g = ((c & 0x7E0) >> 5) << 2;
            uint8_t b = ((c & 0x1F)) << 3;
            ptr[0] = r;
            ptr[1] = g;
            ptr[2] = b;
            ptr[3] = 0xFF;
            ptr += 4;
            srcPtr++;
        }
        srcPtr += padding;
    }

    //printf("%p %d %d %d\n", data, width, height, pitch);
    //printf("video cb\n");
    EM_ASM_({
        drawFrame($0);
    }, rgbaBuffer);
}

bool myEnvironCb(unsigned cmd, void *data)
{
    return false;
}

void myPollCb(void)
{
}

int16_t myInputCb(unsigned port, unsigned device,
                    unsigned index, unsigned id)
{
    return 0;
}

size_t myAudioBatchCb(const int16_t *data, size_t frames)
{
    EM_ASM_({
        writeAudio($0, $1);
    }, data, frames);
    return 0;
}

extern uint8_t rom [0x2000000];

extern "C"
{


    int loadRom(int romSize)
    {

        int ret = retro_load_game(romSize) ? 0 : -1;
        return ret;
    }

    int updateSaveBufState() {
        static uint8_t prevSaveBuf[0x20000 + 0x2000];
        if (memcmp(prevSaveBuf, libretro_save_buf, sizeof(prevSaveBuf)) == 0) {
            return 0;
        }
        memcpy(prevSaveBuf, libretro_save_buf, sizeof(prevSaveBuf));
        return 1;
    }

    void* getBuffer(int bufid) {
        if (bufid == 0) {
            return libretro_save_buf;
        }
        if (bufid == 1) {
            return rom;
        }
        return 0;
    }

    int runFrame(uint32_t joyStatus)
    {
        retro_run(joyStatus);
        return 0;
    }
    
    int resetCpu() 
    {
        retro_reset();
        return 0;
    }

    int main()
    {
        retro_set_audio_sample_batch(myAudioBatchCb);
        retro_set_video_refresh(myVideoCb);
        retro_set_environment(myEnvironCb);
        retro_set_input_poll(myPollCb);
        retro_set_input_state(myInputCb);
        retro_init();
        EM_ASM(wasmReady(););
        return 0;
    }
}
