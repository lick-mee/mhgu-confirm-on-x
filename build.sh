#!/bin/sh
# Builds confirm_on_x.nro.
#   sh build.sh                  -> out/confirm_on_x.nro
#   sh build.sh <recomp folder>  -> also installs it into <recomp folder>/mods/confirm-on-x
#   sh build.sh --zip            -> also packs dist/confirm-on-x-v<version>.zip, the release download
# Needs Zig (on PATH, or ZIG=/path/to/zig) and Python 3. EXTRA_CFLAGS=-DCONFIRM_DEV builds the dev variant.
set -e
cd "$(dirname "$0")"
VERSION=1.0.0
ZIG=${ZIG:-zig}
OUT=out
mkdir -p "$OUT"

CFLAGS="-target arm-freestanding-eabihf -mcpu=cortex_a57 -marm -O2 -fPIC -ffreestanding -fno-builtin
        -fno-stack-protector -fvisibility=hidden -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall
        -DPLUGIN_VERSION=\"$VERSION\" $EXTRA_CFLAGS"
"$ZIG" cc $CFLAGS -c src/plugin.c -o "$OUT/plugin.o"
"$ZIG" cc $CFLAGS -c src/start.S -o "$OUT/start.o"
"$ZIG" ld.lld -shared -T build/nro.ld --no-undefined-version -z undefs --hash-style=sysv \
    -soname confirm_on_x.nro "$OUT/start.o" "$OUT/plugin.o" -o "$OUT/confirm_on_x.elf"
python -I build/elf2nro.py "$OUT/confirm_on_x.elf" "$OUT/confirm_on_x.nro"

case "$1" in
--zip)
    python -I build/pack.py "$OUT/confirm_on_x.nro" mod/README.txt "dist/confirm-on-x-v$VERSION.zip" ;;
"") ;;
*)
    mkdir -p "$1/mods/confirm-on-x/romfs/nativeNX/plugins"
    cp "$OUT/confirm_on_x.nro" "$1/mods/confirm-on-x/romfs/nativeNX/plugins/"
    echo "installed into $1/mods/confirm-on-x" ;;
esac
