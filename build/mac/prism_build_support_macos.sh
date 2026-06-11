#!/bin/bash

##############################################################################
# Unix support functions
##############################################################################
#
# This script file can be included in build scripts for UNIX-compatible
# shells to compose build scripts.
#
##############################################################################

## DEFINE UTILITIES ##

if [ "${TERM-}" -a -z "${CI}" ]; then
    COLOR_RED=$(/usr/bin/tput setaf 1)
    COLOR_GREEN=$(/usr/bin/tput setaf 2)
    COLOR_BLUE=$(/usr/bin/tput setaf 4)
    COLOR_ORANGE=$(/usr/bin/tput setaf 3)
    COLOR_RESET=$(/usr/bin/tput sgr0)
else
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_BLUE=""
    COLOR_ORANGE=""
    COLOR_RESET=""
fi

status() {
    echo -e "${COLOR_BLUE}[${PRODUCT_NAME}] ${1}${COLOR_RESET}"
}

step() {
    echo -e "${COLOR_GREEN}  + ${1}${COLOR_RESET}"
}

info() {
    echo -e "${COLOR_ORANGE}  + ${1}${COLOR_RESET}"
}

error() {
    echo -e "${COLOR_RED}  + ${1}${COLOR_RESET}"
}

exists() {
  /usr/bin/command -v "$1" >/dev/null 2>&1
}

ensure_dir() {
    [ -n "${1}" ] && /bin/mkdir -p "${1}" && builtin cd "${1}"
}

cleanup() {
    :
}

caught_error() {
    error "ERROR during build step: ${1}"
    cleanup
    exit 1
}
