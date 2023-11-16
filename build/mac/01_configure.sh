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
export OBS_BUILD_DIR=${OBS_SRC_DIR}/build
export PRISM_BUILD_DIR=${PRISM_SRC_DIR}/build
export PRISM_VERSION_FILE_DIR=${PROJECT_DIR}/build/mac/version_mac.txt

export GENERATOR=Xcode

if [ -z ${ARCH} ]; then  
  export ARCH="arm64"
fi 

export VERSION="$(sed '1!d' ${PRISM_VERSION_FILE_DIR})"

# increase build version by one
update_mac_version() {
    version_value=${VERSION}
    short_version=${version_value%.*}
    build_version=$(echo $version_value | cut -d'.' -f 4)
}

export PRISM_COMMENTS=

export CEF_ROOT_DIR=${SRC_DIR}/obs-build-dependencies/cef_binary_5060_macos_${ARCH}
export VLC_DIR=${SRC_DIR}/obs-build-dependencies/vlc-3.0.8
export DEPS_DIR=${SRC_DIR}/obs-build-dependencies/obs-deps
export VIRTUALCAM_GUID=5B26DA98-4CA5-40CE-A6DD-FF061AF22A09

if [ -z "${QTDIR}" ]
then
     echo "\$QTDIR is empty, must set your qt path to the environment variable.11"
     exit 1
else
     export ALL_DEPS="${QTDIR};${DEPS_DIR}"
fi

# if [ "${ARCH}"x = "arm64"x  ]; then
# 	MACOS_DEPLOYMENT_TARGET=11.0
# else
# 	MACOS_DEPLOYMENT_TARGET=10.15
# fi
MACOS_DEPLOYMENT_TARGET=12.3

# bundle name
export PRISM_BUNDLE_NAME="PRISMLiveStudio"
export PRISM_PRODUCT_IDENTIFIER="com.prismlive.prismlivestudio"
export PRISM_PRODUCT_IDENTIFIER_PRESUFF="com.prismlive"

export OUTPUT_DIR=${BIN_DIR}/prism/mac/${BUILD_TYPE}
export IGNORE_APNG_BUILD="TRUE"

info "BUILD_TYPE=${BUILD_TYPE}"
info "PROJECT_DIR=${PROJECT_DIR}"
info "BIN_DIR=${BIN_DIR}"
info "SRC_DIR=${SRC_DIR}"
info "OBS_SRC_DIR=${OBS_SRC_DIR}"
info "PRISM_SRC_DIR=${PRISM_SRC_DIR}"
info "OBS_BUILD_DIR=${OBS_BUILD_DIR}"
info "PRISM_BUILD_DIR=${PRISM_BUILD_DIR}"
info "CEF_ROOT_DIR=${CEF_ROOT_DIR}"
info "VLC_DIR=${VLC_DIR}"
info "DEPS_DIR=${DEPS_DIR}"
info "QTDIR=${QTDIR}"
info "VIRTUALCAM_GUID=${VIRTUALCAM_GUID}"
info "ALL_DEPS=${ALL_DEPS}"
info "GENERATOR=${GENERATOR}"
info "COMPILER=${COMPILER}"
info "ARCH=${ARCH}"
info "VERSION=${VERSION}"
info "PRISM_COMMENTS=${PRISM_COMMENTS}"
info "OUTPUT_DIR=${OUTPUT_DIR}"


export PRISM_CODESIGN_TEAM=$(echo "${CODESIGN_IDENT}" | /usr/bin/sed -En "s/.+\((.+)\)/\1/p")

# install macos dependencies

configure-prism() {
	cd ${OBS_SRC_DIR}
	CI/macos/01_install_dependencies.sh

	cd ${PROJECT_DIR}/build/mac

	update_mac_version

	# generator xcode project
	echo \
	cmake -S "${SRC_DIR}" \
	      -B "${PRISM_BUILD_DIR}" \
	      -G "${GENERATOR}" \
		 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
		 -DCEF_ROOT_DIR="${CEF_ROOT_DIR}" \
		 -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET} \
		 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
		 -DRELEASE_CANDIDATE="${VERSION}" \
		 -DENABLE_UI=ON \
		 -DENABLE_BROWSER=ON \
		 -DVLC_PATH="${VLC_DIR}" \
		 -DENABLE_VLC=ON \
		 -DPRISM_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
		 -DPRISM_BUNDLE_CODESIGN_TEAM="${PRISM_CODESIGN_TEAM:--}" \
		 -DSWIG_DIR="${DEPS_DIR}/share/swig/CURRENT" \
		 -DOBS_VERSION_CANONICAL=${short_version} \
		 -DOBS_BUILD_NUMBER=${build_version}


	cmake -S "${SRC_DIR}" \
	      -B "${PRISM_BUILD_DIR}" \
	      -G "${GENERATOR}" \
		 -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		 -DCMAKE_PREFIX_PATH="${ALL_DEPS}" \
		 -DCEF_ROOT_DIR="${CEF_ROOT_DIR}" \
		 -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET} \
		 -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
		 -DRELEASE_CANDIDATE="${VERSION}" \
		 -DENABLE_UI=ON \
		 -DENABLE_BROWSER=ON \
		 -DVLC_PATH="${VLC_DIR}" \
		 -DENABLE_VLC=ON \
		 -DPRISM_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
		 -DPRISM_BUNDLE_CODESIGN_TEAM="${PRISM_CODESIGN_TEAM:--}" \
		 -DSWIG_DIR="${DEPS_DIR}/share/swig/CURRENT" \
		 -DOBS_VERSION_CANONICAL="${short_version}" \
		 -DOBS_BUILD_NUMBER="${build_version}"

	step "PRISM All configuring done"
}


configure-prism-standalone() {

	echo "_RUN_PRISM_BUILD_SCRIPT=${_RUN_PRISM_BUILD_SCRIPT}"

    if [ -z "${_RUN_PRISM_BUILD_SCRIPT}" ]; then
        configure-prism
    fi


}

configure-prism-standalone $*

