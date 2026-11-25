#!/bin/sh
# Builds NTPSet for AmigaOS 2.04+ / any 680x0 (needs ./setup-toolchain.sh once). No C library:
# own startup (start.s), OS calls via vbcc inline stubs, vc.lib only for 68000 32-bit mul/div.
set -e
cd "$(dirname "$0")"
TC=${TC:-$(pwd)/toolchain}
NDK=$TC/NDK3.2
INC="-I../common -I$TC/m68k-amigaos/include -I$NDK/Include_H -I$NDK/SANA+RoadshowTCP-IP/netinclude"
mkdir -p build

for f in ntpset ../common/timecalc; do
  "$TC/bin/vbccm68k" -quiet -c99 -O=1 -cpu=68000 $INC -o=build/$(basename $f).s $f.c
  "$TC/bin/vasmm68k_mot" -quiet -Fhunk -nowarn=62 -m68000 build/$(basename $f).s -o build/$(basename $f).o
done
"$TC/bin/vasmm68k_mot" -quiet -Fhunk -m68000 start.s -o build/start.o
"$TC/bin/vlink" -bamigahunk -x -s -Bstatic -Cvbcc -nostdlib -mrel \
  build/start.o build/ntpset.o build/timecalc.o -L"$TC/m68k-amigaos/lib" -lvc -o NTPSet
ls -l NTPSet

# host unit test of the calendar/DST/NTP math
cc -O2 -Wall -Wextra -I../common -o build/test_timecalc ../common/test/test_timecalc.c ../common/timecalc.c && build/test_timecalc
