#!/bin/sh
rm a.out.*
SRC="libretro-common/memalign.c src/main.cpp src/gba.cpp src/memory.cpp src/sound.cpp src/system.cpp src/thread.cpp libretro/libretro.cpp"

emcc -O3 -s EXPORTED_FUNCTIONS='["_resetCpu", "_updateSaveBufState", "_realloc", "_loadRom","_main", "_runFrame","_getBuffer", "_readU32", "_writeU32"]' -s TOTAL_MEMORY=41943040 $SRC -I src -I libretro-common/include -DFRONTEND_SUPPORTS_RGB565 -DHAVE_STDINT_H -DINLINE=inline -DLSB_FIRST