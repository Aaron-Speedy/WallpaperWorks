#!/bin/bash

set -xe

OS=$(uname | tr '[:upper:]' '[:lower:]')

CURL_VERSION="8.15.0_7"
CURL_DIR="curl-$CURL_VERSION-win64-mingw"
CURL_URL="https://curl.se/windows/dl-8.15.0_7/$CURL_DIR.zip"

rm -rf third_party/real
mkdir third_party/real
cd third_party/real

if [[ "$OS" == "msys_nt"* ]]; then
    curl $CURL_URL --output $CURL_DIR.zip
    unzip $CURL_DIR.zip
    rm $CURL_DIR.zip
    mv $CURL_DIR curl

    cp -r ../freetype .
    (cd freetype/ && ./configure && make)
elif [[ "$OS" == "linux"*  || "$OS" == "darwin" ]]; then
true
else
    echo "Unknown platform. Exiting..."
    exit
fi

cp -r ../libwebp .
(cd libwebp && make -f makefile.unix)
