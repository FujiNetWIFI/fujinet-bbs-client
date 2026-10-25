#!/bin/sh
set -eu
: "${CL65:=cl65}"
: "${FNLIB:=$PWD/fujinet-lib}"
mkdir -p build
"$CL65" -t atari -O -I "$FNLIB/include" -C src/atari.cfg \
 -m build/ataribbs.map -Ln build/ataribbs.lbl -o build/ataribbs.xex \
 src/main.c src/transport.c src/proceed.s src/sio-receive.s src/enhancements.c src/directory.c src/keyboard.s src/boot.s \
 "$FNLIB/r2r/atari/fujinet.atari.lib"
