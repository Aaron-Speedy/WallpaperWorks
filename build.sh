#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11"

OS=$(uname | tr '[:upper:]' '[:lower:]')

LIBWEBP=""

if [[ "$OS" == "windows"* ]]; then
    LIBWEBP="third_party/real/libwebp/lib/libwebp.lib"
elif [[ "$OS" == "linux"* ]]; then
    LIBWEBP="third_party/real/libwebp/src/libwebp.a"
else
    echo "Unknown platform. Exiting..."
    exit
fi

LIBS="\
$LIBWEBP \
-L/usr/X11R6/lib -lX11 \
-lm \
-lcurl \
"

cc src/wallpaper.c -o wallpaper $CFLAGS $LIBS
