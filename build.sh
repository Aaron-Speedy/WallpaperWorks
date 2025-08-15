#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -ggdb -O0 -std=gnu11"

LIBS="\
third_party/libwebp/src/libwebp.a \
-L/usr/X11R6/lib -lX11 \
-lm \
$(pkg-config --cflags freetype2) -lfreetype
"

(cd third_party/libwebp && make -f makefile.unix)
cc wallpaper.c -o wallpaper $CFLAGS $LIBS
