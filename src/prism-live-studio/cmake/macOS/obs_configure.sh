

#!/bin/bash




OBS_ENV=$1
source ${OBS_ENV}

source "${PROJECT_DIR}/build/mac/prism_build_support_macos.sh"
step "\n***************************************start config obs project **************************************\n"

cd ${OBS_SRC_DIR}

echo \
cmake --preset macos \
	 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
	 -DRELEASE_CANDIDATE=${VERSION}  \
	 -DVIRTUALCAM_GUID="${VIRTUALCAM_GUID}" \
	 -DCMAKE_OSX_DEPLOYMENT_TARGET=${PRISM_OSX_DEPLOYMENT_TARGET} \
	 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
	 -DENABLE_WEBSOCKET=ON \
	 -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}"


cmake --preset macos \
	 -Wno-dev \
	 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
	 -DRELEASE_CANDIDATE=${VERSION}  \
	 -DVIRTUALCAM_GUID="${VIRTUALCAM_GUID}" \
	 -DCMAKE_OSX_DEPLOYMENT_TARGET=${PRISM_OSX_DEPLOYMENT_TARGET} \
	 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
	 -DENABLE_WEBSOCKET=ON \
	 -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}"

step "\n***************************************end config obs project **************************************\n" 


