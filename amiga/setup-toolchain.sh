#!/bin/sh
# Fetches and builds the cross toolchain for NTPSet into ./toolchain (host: any Linux, incl. arm64).
#   vbcc  (source, Volker Barthelmann)  -> vbccm68k
#   vasm  (source, Frank Wille)         -> vasmm68k_mot
#   vlink (source, GitHub mirror of Frank Wille's linker)
#   vbcc m68k-amigaos target (startup/vc.lib/proto+inline headers, from the official vbcc bundle)
#   NDK 3.2 from Aminet (OS headers + Roadshow bsdsocket SDK)
# vbcc's build runs "dtgen", which asks a few questions about the HOST's C types; answer them
# for your machine (on Linux/gcc the suggested answers are fine).
set -e
cd "$(dirname "$0")"
mkdir -p toolchain/dl toolchain/src toolchain/bin
cd toolchain

get() { [ -s "dl/$2" ] || curl -fsSL -o "dl/$2" "$1"; }
get http://www.ibaug.de/vbcc/vbcc.tar.gz                                vbcc.tar.gz
get http://www.ibaug.de/vasm/vasm.tar.gz                                vasm.tar.gz
get https://github.com/Leffmann/vlink/archive/refs/heads/master.tar.gz vlink.tar.gz
get http://www.ibaug.de/vbcc/vbcc_linux_x64.tar.gz                      vbcc_bundle.tar.gz
get http://aminet.net/dev/misc/NDK3.2.lha                               NDK3.2.lha

cd src
[ -d vasm ]         || tar xzf ../dl/vasm.tar.gz
[ -d vlink-master ] || tar xzf ../dl/vlink.tar.gz
[ -d vbcc ]         || tar xzf ../dl/vbcc.tar.gz
(cd vasm && mkdir -p obj && make CPU=m68k SYNTAX=mot && cp vasmm68k_mot ../../bin/)
(cd vlink-master && mkdir -p objects && make && cp vlink ../../bin/)
(cd vbcc && mkdir -p bin && make TARGET=m68k && cp bin/vbccm68k ../../bin/)
cd ..

[ -d m68k-amigaos ] || { tar xzf dl/vbcc_bundle.tar.gz vbcc/targets/m68k-amigaos && mv vbcc/targets/m68k-amigaos . && rm -rf vbcc; }
[ -d NDK3.2 ]       || { mkdir NDK3.2 && 7z x -oNDK3.2 dl/NDK3.2.lha >/dev/null; }
echo "toolchain ready in $(pwd)"
