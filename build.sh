#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11"

OS=$(uname | tr '[:upper:]' '[:lower:]')

LIBWEBP=""
CURL=""

if [[ "$OS" == "windows"* ]]; then
    LIBWEBP="third_party/real/libwebp/lib/libwebp.lib"
    CURL="third_party/real/curl/lib/libcurl.a"
elif [[ "$OS" == "linux"* ]]; then
    LIBWEBP="third_party/real/libwebp/src/libwebp.a"
    CURL="-lcurl"
else
    echo "Unknown platform. Exiting..."
    exit
fi

LIBS="\
$LIBWEBP \
-L/usr/X11R6/lib -lX11 \
-lm \
$CURL \
"

cc src/wallpaper.c -o wallpaper $CFLAGS $LIBS
