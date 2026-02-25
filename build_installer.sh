#!/bin/sh

set -e

NSIS=${1:-"C:\Program Files (x86)\NSIS\makensis.exe"}

if [ ! -f "$NSIS" ]; then
    echo \
"Error: Unable to access \"$NSIS\". Supply an alternative path with \
\`\`$0 /path/to/makensis.exe\`\`, or install NSIS to the default \
path with the installer, which you can download from \
https://nsis.sourceforge.io/Download. \
"
    exit 1
fi

if [ ! -d "build" ]; then
    echo "Error: You must run \`\`./build.sh\`\` before making the installer."
    exit 1
fi

"$NSIS" src/installer.nsi
