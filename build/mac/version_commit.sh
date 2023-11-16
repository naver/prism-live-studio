#!/bin/bash

set -eE

export _RUN_PRISM_BUILD_SCRIPT=TRUE

SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
source "${SCRIPT_PATH}/prism_build_support_macos.sh"
cd $SCRIPT_PATH
CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"

# increase build version by one
update_mac_version() {
    status "update_mac_version"
    verson_file_path=${CHECKOUT_DIR}/build/mac/version_mac.txt
    version_value=$(sed '1!d' ${verson_file_path})
    short_version=${version_value%.*}
    build_version=$(echo $version_value | cut -d'.' -f 4)
    new_build_version=$(($((10#$build_version))+1))
    new_full_version=$short_version'.'$new_build_version
    echo "$new_full_version" | tee ${verson_file_path}
    info $new_full_version	
}


push_commit() {
	cd $CHECKOUT_DIR
    verson_file_path=${CHECKOUT_DIR}/build/mac/version_mac.txt
	VERSION="$(sed '1!d' ${verson_file_path})"
	status $VERSION
	status $ARCHS
    trap "caught_error 'git commit'" ERR
    git add "${verson_file_path}"
    git commit -m "[CI]Auto increase Mac build number to $VERSION. arch=${ARCHS}"
    git_branch=$(git branch --show-current)
    status "current barnch is ${git_branch}"
    git checkout .
    git pull origin ${git_branch} -r
    git push origin ${git_branch}
}

prism-version-main() {
    while true; do
        case "${1}" in
            -v | --version ) export ADD_VERSION=TRUE; shift ;;
            -p | --push ) export PUSH_GIT=TRUE; shift ;;
            -a | --architecture ) export ARCHS="${2}"; shift 2 ;;
            -c | --clean ) export GIT_CLEAN=TRUE; shift ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    if [ "${ADD_VERSION}" ]; then
        trap "caught_error 'add mac version'" ERR
        update_mac_version
    fi
    echo ${PUSH_GIT}
    if [ "${PUSH_GIT}" ]; then
        push_commit
    fi

    echo ${GIT_CLEAN}
    if [ "${GIT_CLEAN}" ]; then
        cd $CHECKOUT_DIR
        git checkout .
    fi
}

prism-version-main $*