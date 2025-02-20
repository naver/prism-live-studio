#!/bin/bash

# BUILD_TYPE=Debug, Release, RelWithDebInfo and MinSizeRel,

buildTypes=(
Debug
Release
RelWithDebInfo
MinSizeRel
)

if [ -z ${BUILD_TYPE} ]; then  
  if [ -z ${1} ]; then  
  	export BUILD_TYPE=Debug  
  else 	 
	export BUILD_TYPE=${1}  
  fi 
fi 

if [[ ! "${buildTypes[@]}"  =~ "${BUILD_TYPE}" ]]; then
	echo "BUILD_TYPE: ${BUILD_TYPE} is not valid"
	exit 1
fi

SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
export PROJECT_DIR="$SCRIPT_PATH/../.."
source "${PROJECT_DIR}/build/mac/prism_build_support_macos.sh"


export BIN_DIR=${PROJECT_DIR}/bin
export SRC_DIR=${PROJECT_DIR}/src

export OBS_SRC_DIR=${SRC_DIR}/obs-studio
export PRISM_SRC_DIR=${SRC_DIR}/prism-live-studio
export OBS_BUILD_DIR=${OBS_SRC_DIR}/build_macos
export PRISM_BUILD_DIR=${PRISM_SRC_DIR}/build
export PRISM_VERSION_FILE_DIR=${PROJECT_DIR}/build/mac/version_mac.txt
export OBS_VERSION=30.1.2.0

export GENERATOR=Xcode

if [ -z ${ARCH} ]; then  
  export ARCH="arm64"
fi 

if [ -z ${VERSION} ]; then  
  export VERSION="$(sed '1!d' ${PRISM_VERSION_FILE_DIR})"
else
	echo ${VERSION}  > ${PRISM_VERSION_FILE_DIR}
fi 

# increase build version by one
update_mac_version() {
    version_value=${VERSION}
    short_version=${version_value%.*}
    build_version=$(echo $version_value | cut -d'.' -f 4)
}

export VIRTUALCAM_GUID=5B26DA98-4CA5-40CE-A6DD-FF061AF22A09

export PRISM_QT_DIR="${QTDIR}"
if [ -z "${PRISM_QT_DIR}" ]
then
	if [ -z "${QT653}" ]
		then
			error "\$QTDIR or QT653 is empty, must set your qt path to the environment variable."
			exit 1
	else
		PRISM_QT_DIR="${QT653}"
	fi
fi

export ALL_DEPS="${PRISM_QT_DIR}"
MACOS_DEPLOYMENT_TARGET=12.3

# bundle name
export PRISM_BUNDLE_NAME="PRISMLiveStudio"
export PRISM_PRODUCT_IDENTIFIER="com.prismlive.prismlivestudio"
export PRISM_PRODUCT_IDENTIFIER_PRESUFF="com.prismlive"

export OUTPUT_DIR=${BIN_DIR}/prism/mac/${BUILD_TYPE}

info "BUILD_TYPE=${BUILD_TYPE}"
info "PROJECT_DIR=${PROJECT_DIR}"
info "BIN_DIR=${BIN_DIR}"
info "SRC_DIR=${SRC_DIR}"
info "OBS_SRC_DIR=${OBS_SRC_DIR}"
info "PRISM_SRC_DIR=${PRISM_SRC_DIR}"
info "OBS_BUILD_DIR=${OBS_BUILD_DIR}"
info "PRISM_BUILD_DIR=${PRISM_BUILD_DIR}"
info "VLC_DIR=${VLC_DIR}"
info "VIRTUALCAM_GUID=${VIRTUALCAM_GUID}"
info "ALL_DEPS=${ALL_DEPS}"
info "GENERATOR=${GENERATOR}"
info "ARCH=${ARCH}"
info "VERSION=${VERSION}"
info "OUTPUT_DIR=${OUTPUT_DIR}"


export PRISM_CODESIGN_TEAM=$(echo "${CODESIGN_IDENT}" | /usr/bin/sed -En "s/.+\((.+)\)/\1/p")

# install macos dependencies

configure-prism() {
	cd ${OBS_SRC_DIR}
	cd ${PROJECT_DIR}/build/mac

	update_mac_version
	# generator xcode project
	echo \
	cmake -S "${SRC_DIR}" \
	      -B "${PRISM_BUILD_DIR}" \
	      -G "${GENERATOR}" \
		 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
		 -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET} \
		 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
		 -DRELEASE_CANDIDATE="${VERSION}" \
		 -DPRISM_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
		 -DPRISM_BUNDLE_CODESIGN_TEAM="${PRISM_CODESIGN_TEAM:--}" \
		 -DPRISM_VERSION_SHORT="${short_version}" \
		 -DPRISM_VERSION_BUILD="${build_version}"\
         -DENABLE_TEST="${ENABLE_TEST:-OFF}"

# --log-level=DEBUG \
	cmake -Wno-dev \
	 -S "${SRC_DIR}" \
	      -B "${PRISM_BUILD_DIR}" \
	      -G "${GENERATOR}" \
		 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
		 -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET} \
		 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
		 -DRELEASE_CANDIDATE="${VERSION}" \
		 -DPRISM_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
		 -DPRISM_BUNDLE_CODESIGN_TEAM="${PRISM_CODESIGN_TEAM:--}" \
		 -DPRISM_VERSION_SHORT="${short_version}" \
		 -DPRISM_VERSION_BUILD="${build_version}" \
         -DENABLE_TEST="${ENABLE_TEST:-OFF}"
	step "PRISM All configuring done"
}


configure-prism-standalone() {

	echo "_RUN_PRISM_BUILD_SCRIPT=${_RUN_PRISM_BUILD_SCRIPT}"

    if [ -z "${_RUN_PRISM_BUILD_SCRIPT}" ]; then
        configure-prism
    fi


}

configure-prism-standalone $*

