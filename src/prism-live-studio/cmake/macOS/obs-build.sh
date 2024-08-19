
#!/bin/bash

echo ***********prism to build obs project start*******************************************************************

export OBS_ENV=$1
export CURRENT_BUILD_TYPE=$2

source ${OBS_ENV}

# echo ******************************************************************************
# echo "cmake --build ${OBS_BUILD_DIR} --target ALL_BUILD --config ${CURRENT_BUILD_TYPE} ${CLEAN_FIRST}"
# echo ******************************************************************************

# cmake --build ${OBS_BUILD_DIR} --target ALL_BUILD --config ${CURRENT_BUILD_TYPE} ${CLEAN_FIRST}
# /opt/homebrew/bin/cmake --build ${OBS_BUILD_DIR} --target ALL_BUILD --config ${CURRENT_BUILD_TYPE} ${CLEAN_FIRST}

echo "cd ${OBS_BUILD_DIR}"
cd ${OBS_BUILD_DIR}

echo 'xcodebuild build -configuration ${CURRENT_BUILD_TYPE} -scheme obs-studio -parallelizeTargets -destination "generic/platform=macOS,name=Any Mac"'

xcodebuild build -configuration ${CURRENT_BUILD_TYPE} -scheme obs-studio -parallelizeTargets -destination "generic/platform=macOS,name=Any Mac"

echo ***********prism to build obs project end*******************************************************************