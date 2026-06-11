#!/bin/bash
# filepath: apply_patches.sh

# Save current directory
CURRENT_DIR=$(pwd)
PLUGIN_PATCHES_DIR="${OBS_SRC_DIR}/plugins/patches"
echo "PLUGIN_PATCHES_DIR: ${PLUGIN_PATCHES_DIR}"

# apply obs-websocket patches start
PLUGIN_OBS_WEBSOCKET_REPOSITORY_DIR="${OBS_SRC_DIR}/plugins/obs-websocket"
WEBSOCKET_PATCH_DIR="${PLUGIN_PATCHES_DIR}/obs-websocket"
./apply_patches_sub.sh "${PLUGIN_OBS_WEBSOCKET_REPOSITORY_DIR}" "${WEBSOCKET_PATCH_DIR}"
if [ $? -ne 0 ]; then
  echo -e "\033[31mFailed to apply obs-websocket patches\033[0m"
  exit 1
fi
# apply obs-websocket patches stop
exit 0