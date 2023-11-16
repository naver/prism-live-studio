#!/bin/bash

APNG_BUILD_PATH=$1
APNG_QT_PATH=$2
APNG_ARCH=$3

SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);

#only build release type
cd $APNG_BUILD_PATH
"${APNG_QT_PATH}/bin/qmake" "${SCRIPT_PATH}/qtapng.pro" "CONFIG+=release" "CONFIG+=qtquickcompiler" QMAKE_APPLE_DEVICE_ARCHS=${APNG_ARCH}
make -j $(nproc)

codesign --force -s - "${APNG_BUILD_PATH}/plugins/imageformats/libqapng.dylib"

echo "apng dylib output temp direct is" $APNG_BUILD_PATH
