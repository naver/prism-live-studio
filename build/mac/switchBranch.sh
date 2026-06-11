#!/bin/bash

set -eE

statsh_single_branch() {
  step "stash: cd to $1"
  trap "caught_error 'stash $1'" ERR

  if [ ! -d "${PROJECT_DIR}$1" ]; then
      status "${PROJECT_DIR}$1 not exist"
      return
  fi

  cd "${PROJECT_DIR}$1"
  git stash -m "auto stash by switch_branch"

}

switch_single_branch() {
  
  step "pull: cd to $1"
  trap "caught_error 'pull $1'" ERR

  cd "${PROJECT_DIR}$1"

  if [ "$1" == "/src/obs-studio" ]; then
    git submodule update --init --recursive
  fi
  git fetch origin $2
  git checkout $2
  git pull origin $2 -r

}

switch_branch() {

  SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
  source "${SCRIPT_PATH}/prism_build_support_macos.sh"

  if [ -z ${branch_prism} ]; then 
    if [ ${tmp} ]; then  
        branch_prism=${tmp}
    else
        branch_prism=develop
    fi  
  fi 

  if [ -z ${branch_obs} ]; then  
    branch_obs=${branch_prism}
  fi 

  if [ -z ${branch_obs_browser} ]; then  
    branch_obs_browser=${branch_prism}
  fi 

  info "tmp=${tmp}"
  info "branch_prism=${branch_prism}"
  info "branch_obs=${branch_obs}"
  info "branch_obs_browser=${branch_obs_browser}"

  cd $SCRIPT_PATH
  export PROJECT_DIR="$(git rev-parse --show-toplevel)"
  cd $PROJECT_DIR

  path_prism=""
  path_obs_studio="/src/obs-studio"
  path_obs_browser="/src/obs-studio/plugins/obs-browser"


  statsh_single_branch "$path_prism"
  statsh_single_branch "$path_obs_studio"
  statsh_single_branch "$path_obs_browser"

  git submodule update --init --recursive

  switch_single_branch "$path_prism" "${branch_prism}"
  switch_single_branch "$path_obs_studio" ${branch_obs}

  cd "${PROJECT_DIR}/$path_obs_studio"
  git submodule update
  
  switch_single_branch "$path_obs_browser" ${branch_obs_browser}
}



prism-build-main() {
    tmp=${1}
    while true; do
        case "${1}" in
            -p | --prism ) export branch_prism="${2}"; shift 2 ;;
            -o | --obs ) export branch_obs="${2}"; shift 2 ;;
            -b | --obs-browser ) export branch_obs_browser="${2}"; shift 2 ;;
            -- ) shift; branch_other="${2}" break ;;
            * ) break ;;
        esac
    done

    switch_branch
}

prism-build-main $*
