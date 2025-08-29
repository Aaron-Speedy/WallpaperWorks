#!/bin/sh

set -xe

OS=$(uname | tr '[:upper:]' '[:lower:]')

webpwin="libwebp-1.6.0-windows-x64"
webpurlwin="https://storage.googleapis.com/downloads.webmproject.org/releases/webp/$webpwin.zip"

curlwin="curl-8.15.0_5-win64-mingw"
curlurlwin="https://curl.se/windows/dl-8.15.0_5/$curlwin.zip"

rm -rf third_party/real
mkdir third_party/real
cd third_party/real

if [[ "$OS" == "windows"* ]]; then
    curl $webpurlwin --output libwebp.zip
    unzip libwebp.zip
    rm libwebp.zip
    mv $webpwin libwebp

    curl $curlurlwin --output curl.zip
    unzip curl.zip
    rm curl.zip
    mv $curlwin curl
elif [[ "$OS" == "linux"* ]]; then
    cp -r ../libwebp .
    (cd libwebp && make -f makefile.unix)
else
    echo "Unknown platform. Exiting..."
    exit
fi
