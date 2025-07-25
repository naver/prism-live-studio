@echo off

setlocal

set OBS_ENV=%1

call %OBS_ENV%

echo obs configure.

cmake -Wno-dev ^
    -S "%OBS_SRC_DIR%" ^
    -B "%OBS_BUILD_DIR%" ^
    -G "%GENERATOR%" ^
    --preset windows-x64 ^
    -T v143 ^
    -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
    -DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
    -DENABLE_UI=ON ^
    -DENABLE_BROWSER=ON ^
    -DENABLE_SCRIPTING=ON ^
    -DENABLE_SCRIPTING_LUA=ON ^
    -DENABLE_SCRIPTING_PYTHON=ON
