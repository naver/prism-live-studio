#!/bin/bash

##############################################################################
# macOS libprism plugin package function
##############################################################################
#
# This script file can be included in build scripts for macOS or run directly
#
##############################################################################

# Halt on errors
set -eE

get_build_file_name() {
    VERSION_STRING=$(sed '1!d' ${PRISM_VERSION_FILE_DIR})
    FILE_NAME="${PRISM_BUNDLE_NAME}_macos_${ARCH}_${VERSION_STRING}.dmg"
}

package_prism() {
    status "Create macOS disk image"
}

notarize_prism() {
    status "Notarize PRISMLiveStudio"
}

move_to_deploy() {
    PRISMP_DEPLOY_MAC=$1
    step "move_to_deploy: ${NOTARIZE_TARGET} to ${PRISMP_DEPLOY_MAC}"
    ensure_dir "${PRISMP_DEPLOY_MAC}"
    cp "${NOTARIZE_TARGET}" "${PRISMP_DEPLOY_MAC}"
}

_caught_error_hdiutil_verify() {
    error "ERROR during verifying image '${1}'"
    cleanup
    exit 1
}
