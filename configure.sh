#!/bin/sh

set -xe

OS=$(uname | tr '[:upper:]' '[:lower:]')
webpwin="libwebp-1.6.0-windows-x64"
webpurlwin="https://storage.googleapis.com/downloads.webmproject.org/releases/webp/$webpwin.zip"

rm -r third_party/real
mkdir third_party/real
cd third_party/real

if [[ "$OS" == "windows"* ]]; then
    curl $webpurlwin --output libwebp.zip
    unzip libwebp.zip
    rm libwebp.zip
    mv $webpwin libwebp
elif [[ "$OS" == "linux"* ]]; then
    cp -r ../libwebp .
    (cd libwebp && make -f makefile.unix)
else
    echo "Unknown platform. Exiting..."
    exit
fi
