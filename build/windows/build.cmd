@echo off

setlocal

if /I "%1"=="Debug" (
	set BUILD_TYPE_ARG=Debug
) else (
	set BUILD_TYPE_ARG=Release
)
if "%BUILD_TYPE_ARG%"=="Debug" (
	set CMAKE_BUILD_TYPE=Debug
) else (
	set CMAKE_BUILD_TYPE=RelWithDebInfo
)

set _PROJECT_DIR=%~dp0..\..
set PROJECT_DIR=%_PROJECT_DIR:\=/%
set SRC_DIR=%PROJECT_DIR%/src
set OBS_BUILD_DIR=%SRC_DIR%/obs-studio/build/%BUILD_TYPE_ARG%
set PRISM_BUILD_DIR=%SRC_DIR%/prism-live-studio/build/%BUILD_TYPE_ARG%

if "%BUILD_RANGE_ARG%"=="" (
	set BUILD_RANGE=rebuild
) else (
	set BUILD_RANGE=%BUILD_RANGE_ARG%
)


if EXIST "%OBS_BUILD_DIR%\deps\obs-scripting\obslua\CMakeFiles\obslua.dir\obsluaLUA_wrap.c" cmake -E remove -f "%OBS_BUILD_DIR%\deps\obs-scripting\obslua\CMakeFiles\obslua.dir\obsluaLUA_wrap.c"
if EXIST "%OBS_BUILD_DIR%\deps\obs-scripting\\obspython\CMakeFiles\_obspython.dir\obspythonPYTHON_wrap.c" cmake -E remove -f "%OBS_BUILD_DIR%\deps\obs-scripting\\obspython\CMakeFiles\_obspython.dir\obspythonPYTHON_wrap.c"

if "%BUILD_RANGE%"=="rebuild" (
    echo clear previous build rundir.
    set OBS_BUILD_RUNDIR=%OBS_BUILD_DIR%\rundir
    if EXIST "%OBS_BUILD_RUNDIR%" cmake -E remove_directory "%OBS_BUILD_RUNDIR%"
    set PRISM_BUILD_RUNDIR=%PRISM_BUILD_DIR%\rundir
    if EXIST "%PRISM_BUILD_RUNDIR%" cmake -E remove_directory "%PRISM_BUILD_RUNDIR%"

    echo rebuild PRISMLiveStudio start.

    echo ##################################################
    echo ########## Build 32 bits Virtual Camera ##########
    echo ##################################################
    echo cmake --build "%PRISM_BUILD_DIR%" --target ALL_BUILD --config %CMAKE_BUILD_TYPE% --clean-first --parallel 16  -- /p:CL_MPcount=16
    cmake --build "%PRISM_BUILD_DIR%" --target ALL_BUILD --config %CMAKE_BUILD_TYPE% --clean-first --parallel 16  -- /p:CL_MPcount=16
) else (
    echo build PRISMLiveStudio start.

    echo ##################################################
    echo ########## Build 32 bits Virtual Camera ##########
    echo ##################################################
    echo cmake --build "%PRISM_BUILD_DIR%" --target ALL_BUILD --config %CMAKE_BUILD_TYPE% --parallel 16 -- /p:CL_MPcount=16
    cmake --build "%PRISM_BUILD_DIR%" --target ALL_BUILD --config %CMAKE_BUILD_TYPE% --parallel 16 -- /p:CL_MPcount=16
)

echo build PRISMLiveStudio complete.
