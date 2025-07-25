#!/bin/bash
# filepath: apply_patches_sub.sh

REPOSITORY_DIR=$1
PATCH_DIR=$2

echo -e "\033[33mstart apply patches: $PATCH_DIR\033[0m"

# Save current directory
CURRENT_DIR=$(pwd)

# cd to the submodule dir
cd "$REPOSITORY_DIR" || { echo -e "\033[31mfailed to change to repository directory\033[0m"; exit 1; }

# stash the modifications first
git stash

# apply the patches
find "$PATCH_DIR" -name "*.patch" -type f | sort | while read -r patch_file; do
  echo -e "\033[33mapply patch: $patch_file\033[0m"
  git apply "$patch_file"
  if [ $? -ne 0 ]; then
    echo -e "\033[31mfailed to apply patches\033[0m"
    exit 1
  fi
done

# Check if any patch application failed (catching the exit from within the loop)
if [ $? -ne 0 ]; then
  exit 1
fi

# back to the CURRENT_DIR
echo -e "\033[32mapply patches completely: $PATCH_DIR\033[0m"
cd "$CURRENT_DIR" || { echo -e "\033[31mfailed to return to original directory\033[0m"; exit 1; }
exit 0