#!/bin/sh
# builds NTPSET.EXE for Win95/98 (needs gcc-mingw-w64-i686); no C runtime, kernel32/user32/wsock32 only
cd "$(dirname "$0")"
i686-w64-mingw32-gcc -Os -s -march=i386 -mtune=pentium3 -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -ffreestanding -mwindows -nostdlib -nostartfiles -Wall -Wextra \
  -Wl,-e,_entry@0 -Wl,--major-subsystem-version,4,--minor-subsystem-version,0 \
  -Wl,--major-os-version,4,--minor-os-version,0 -Wl,--disable-dynamicbase,--disable-nxcompat \
  -o NTPSET.EXE ntpset.c -lwsock32 -lkernel32 -luser32
