#!/bin/bash

#ARCH: arm64 | x86_64 | universal
# bash build-macos.sh --ci --build-type RelWithDebInfo -a arm64 --temp

set -eE

export _RUN_PRISM_BUILD_SCRIPT=TRUE

SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
source "${SCRIPT_PATH}/prism_build_support_macos.sh"
cd $SCRIPT_PATH
CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"

print_usage() {
    echo "build-macos.sh - Build script for OBS-Studio"
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-a, --architecture             : Specify build architecture (default: x86_64, alternative: arm64)\n" \
            "-b, --bundle                   : Create relocatable application bundle (default: off)\n" \
            "-p, --package                  : Create distributable disk image (default: off)\n" \
            "-c, --codesign                 : Codesign OBS and all libraries (default: ad-hoc only)\n" \
            "-n, --notarize                 : Notarize OBS (default: off)\n"


        print_deprecation
}

print_deprecation() {
    echo -e "PRISM set environment variable:\n" \
"build require: export QTDIR=\"xxxx/6.3.2/macos\" \n \

package require:\n \
    export CODESIGN_IDENT=\"Developer ID Application: xxxx. (K9UPxxx)\"\n\
    export PRISM_BUNDLE_PP=\"xxx pp file\"\n\

notary require:\n \
    /usr/bin/xcrun notarytool store-credentials PRSIM-Codesign-Password --apple-id xxx@navercorp.com --team-id K9UPxxx --password xxxxx"

}

delete-build-path() {
    if [ "${CLEAN}" ]; then
        echo `ls -ld ${PRISM_BUILD_DIR}`
        echo `ls -ld ${PRISM_BUILD_DIR}/build`
        echo `ls -ld ${OBS_BUILD_DIR}`
        echo `ls -ld ${OBS_BUILD_DIR}/build`
        echo "current login user: ${USER}"

        (
            set +e
            echo "first try delete build path"
            rm -rf ${PRISM_BUILD_DIR}
            rm -rf ${OBS_BUILD_DIR}
        ) || {
            echo "second try delete build path"
            rm -rf ${PRISM_BUILD_DIR}
            rm -rf ${OBS_BUILD_DIR}
        }
    fi
}


setup_default_config() {
    info "CIPackage = ${CIPackage}"
    info "PACKAGE = ${PACKAGE}"
    info "BUILD = ${BUILD}"
    info "BUNDLE = ${BUNDLE}"
    info "CITEMP = ${CITEMP}"
    info "NOTARIZE = ${NOTARIZE}"
    info "CLEAN = ${CLEAN}"

    source "${SCRIPT_PATH}/01_configure.sh"

    delete-build-path

    info "PROJECT_DIR = ${PROJECT_DIR}"
    cd ${PROJECT_DIR}

    if [ "${CIPackage}" ]; then
        trap "caught_error 'python download sync json'" ERR
           if [ "${IGNORE_SYNC}" ]; then
            info "ignore download sync by command line"
        else
            /usr/bin/python3 "${SCRIPT_PATH}/../common/downSyncJson.py" --path="${PRISM_SRC_DIR}/PRISMLiveStudio" -v=${VERSION} ${PRISM_DEV_PYTHON_DOWNLOAD}
        fi
        

        trap "caught_error 'python download gpop'" ERR
        if [ "${IGNORE_GPOP}" ]; then
            info "ignore download gpop by command line"
        else
            /usr/bin/python3 "${SCRIPT_PATH}/../common/downloadGpop.py" "${PRISM_SRC_DIR}/PRISMLiveStudio/prism-ui/resource/DefaultResources/mac/gpop.json" ${VERSION} "mac" ${PRISM_DEV_PYTHON_DOWNLOAD}
        fi
    fi

    info "BUILD_TYPE=${BUILD_TYPE}"
    configure-prism
}

prism-build-main() {
    while true; do
        case "${1}" in
            -h | --help ) print_usage; exit 0 ;;
            -a | --architecture ) export ARCH="${2}"; shift 2 ;;
            -n | --notarize ) NOTARIZE=TRUE; shift ;;
            -b | --bundle ) BUNDLE=TRUE; shift ;;
            --ci ) CIPackage=TRUE; PACKAGE=TRUE; BUILD=TRUE; IGNORE_COMMIT=FALSE;  NOTARIZE=TRUE; CLEAN=TRUE; shift ;;   
            --build ) BUILD=TRUE; shift ;;   
            --clean ) CLEAN=TRUE; shift ;;   
            --build-type ) export BUILD_TYPE="${2}"; shift 2 ;;
            --ignore-sync ) IGNORE_SYNC=TRUE; shift ;;
            --ignore-gpop ) IGNORE_GPOP=TRUE; shift ;;
            -t | --temp ) CITEMP=TRUE; shift ;;
            --dev-download) PRISM_DEV_PYTHON_DOWNLOAD="--dev"; shift ;;
            --test ) export ENABLE_TEST=ON; shift ;;
             -v | --version ) export VERSION="${2}"; shift 2 ;;
            --qt-dir ) export QTDIR="${2}"; shift 2 ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    setup_default_config

    if [ "${BUILD}" ]; then
        trap "caught_error 'build prism app'" ERR
        cd ${PRISM_SRC_DIR}
        cmake --build ${PRISM_BUILD_DIR} --config ${BUILD_TYPE} -- -j $(nproc)
    fi

    if [ "${BUNDLE}" ]; then
        trap "caught_error 'install prism app'" ERR
        cd ${PRISM_SRC_DIR}
        cmake --install ${PRISM_BUILD_DIR} --config ${BUILD_TYPE}
    fi

    step "build-macos script done."
}

prism-build-main $*
