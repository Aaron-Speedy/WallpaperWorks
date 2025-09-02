#!/bin/sh

set -xe

OS=$(uname | tr '[:upper:]' '[:lower:]')

WEBPWIN="libwebp-1.6.0-windows-x64"
WEBPURLWIN="https://storage.googleapis.com/downloads.webmproject.org/releases/webp/$WEBPWIN.zip"

CURLWIN="curl-8.15.0_5-win64-mingw"
CURLURLWIN="https://curl.se/windows/dl-8.15.0_5/$CURLWIN.zip"

rm -rf third_party/real
mkdir third_party/real
cd third_party/real

if [[ "$OS" == "windows"* ]]; then
    curl $WEBPURLWIN --output libwebp.zip
    unzip libwebp.zip
    rm libwebp.zip
    mv $WEBPWIN libwebp

    curl $CURLURLWIN --output curl.zip
    unzip curl.zip
    rm curl.zip
    mv $CURLWIN curl

    cp -r ../freetype .
    (cd freetype/ && ./configure && make)
elif [[ "$OS" == "linux"* ]]; then
    cp -r ../libwebp .
    (cd libwebp && make -f makefile.unix)
else
    echo "Unknown platform. Exiting..."
    exit
fi
