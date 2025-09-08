#!/bin/sh

set -xe

rm -rf build
mkdir build

cp -r resources/* build/

# TODO: Remove -Wno-unused-parameter
CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11"

OS=$(uname | tr '[:upper:]' '[:lower:]')

LIBWEBP=""
CURL=""
WINDOWING=""
FREETYPE=""
OTHER=""

CURL_DIR="third_party/real/curl"
FT_DIR="third_party/real/freetype"
WEBP_DIR="third_party/real/webp"

if [[ "$OS" == "windows"* ]]; then
    LIBWEBP="third_party/real/libwebp/src/libwebp.a"
    cp $CURL_DIR/bin/libcurl-x64.dll build/
    cp $CURL_DIR/bin/curl-ca-bundle.crt build/
    CURL="-L$CURL_DIR/lib/ -I$CURL_DIR/include/ -lcurl"

    WINDOWING="-mwindows"
    FREETYPE="-I$FT_DIR/include/ $FT_DIR/objs/.libs/libfreetype.a"

    windres src/recs.rc -O coff -o build/recs.res
    OTHER="build/recs.res"
elif [[ "$OS" == "linux"* ]]; then
    LIBWEBP="$WEBP_DIR/src/libwebp.a"
    CURL="-lcurl"
    WINDOWING="-L/usr/X11R6/lib -lX11"
    FREETYPE="$(pkg-config --cflags freetype2) -lfreetype"
else
    echo "Unknown platform. Exiting..."
    exit
fi

LIBS="\
-lm -lpthread \
$LIBWEBP \
$WINDOWING \
$CURL \
$FREETYPE \
"

cc src/wallpaper.c -o build/wallpaper $CFLAGS $LIBS $OTHER
# cc test.c -o build/test $CFLAGS $LIBS
