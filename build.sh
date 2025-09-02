#!/bin/sh

set -xe

# TODO: Remove -Wno-unused-parameter
CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11 -Wno-unused-parameter"

OS=$(uname | tr '[:upper:]' '[:lower:]')

LIBWEBP=""
CURL=""
WINDOWING=""
FREETYPE=""

if [[ "$OS" == "windows"* ]]; then
    LIBWEBP="third_party/real/libwebp/lib/libwebp.lib"
    CURL="third_party/real/curl/lib/libcurl.a -Ithird_party/real/curl/include/"
    WINDOWING="-mwindows"
    FREETYPE="-Ithird_party/real/freetype/include/ third_party/real/freetype/objs/.libs/libfreetype.a"
elif [[ "$OS" == "linux"* ]]; then
    LIBWEBP="third_party/real/libwebp/src/libwebp.a"
    CURL="-lcurl"
    WINDOWING="-L/usr/X11R6/lib -lX11"
    FREETYPE="$(pkg-config --cflags freetype2) -lfreetype"
else
    echo "Unknown platform. Exiting..."
    exit
fi

LIBS="\
-lm \
$LIBWEBP \
$WINDOWING \
$CURL \
$FREETYPE \
"

rm -rf build
mkdir build
# cc src/wallpaper.c -o build/wallpaper $CFLAGS $LIBS
cc test.c -o build/test $CFLAGS $LIBS
