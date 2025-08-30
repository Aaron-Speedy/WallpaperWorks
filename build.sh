#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11"

OS=$(uname | tr '[:upper:]' '[:lower:]')

LIBWEBP=""
CURL=""
WINDOWING=""

if [[ "$OS" == "windows"* ]]; then
    LIBWEBP="third_party/real/libwebp/lib/libwebp.lib"
    CURL="third_party/real/curl/lib/libcurl.a"
    WINDOWING="-mwindows"
elif [[ "$OS" == "linux"* ]]; then
    LIBWEBP="third_party/real/libwebp/src/libwebp.a"
    CURL="-lcurl"
    WINDOWING="-L/usr/X11R6/lib -lX11"
else
    echo "Unknown platform. Exiting..."
    exit
fi

LIBS="\
-lm \
$LIBWEBP \
$WINDOWING \
$CURL \
"

rm -rf build
mkdir build
# cc src/wallpaper.c -o build/wallpaper $CFLAGS $LIBS
cc test.c -o build/test $CFLAGS $LIBS
