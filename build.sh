#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -ggdb -O3 -std=gnu11"

(cd libwebp && make -f makefile.unix)
cc wallpaper.c -o wallpaper $CFLAGS "libwebp/src/libwebp.a" -L/usr/X11R6/lib -lX11
