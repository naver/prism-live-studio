@echo off

setlocal

set OBS_ENV=%1
set CURRENT_BUILD_TYPE=%2

call %OBS_ENV%

echo obs build.
echo ******************************************************************************
echo cmake --build "%OBS_BUILD_DIR%" --target ALL_BUILD --config %CURRENT_BUILD_TYPE% %CLEAN_FIRST% --parallel 16 -- /p:CL_MPcount=16
echo ******************************************************************************
cmake --build "%OBS_BUILD_DIR%" --target ALL_BUILD --config %CURRENT_BUILD_TYPE% %CLEAN_FIRST% --parallel 16 -- /p:CL_MPcount=16
