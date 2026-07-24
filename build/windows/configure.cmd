@echo off

setlocal

cd %~dp0

if /I "%1"=="Debug" (
	set QTDIR=%QTDIR_Debug%
	set BUILD_TYPE_ARG=Debug
) else (
	set QTDIR=%QTDIR_RelWithDebInfo%
	set BUILD_TYPE_ARG=Release
)

set ENABLE_TEST=OFF
if "%PACK_TYPE_ARG%"=="test" (
	set ENABLE_TEST=ON
) else (
	for %%i in (%*) do (
		if "%%i"=="--test" set ENABLE_TEST=ON
	)
)

if "%PERFORMANCE_STATS_ARG%"=="" (
	set PERFORMANCE_STATS_ARG=OFF
	for %%i in (%*) do (
		if "%%i"=="--performance-stats" set PERFORMANCE_STATS_ARG=ON
	)
)

if "%UI_ACTION_STATS_ARG%"=="" (
	set UI_ACTION_STATS_ARG=OFF
	for %%i in (%*) do (
		if "%%i"=="--ui-action-stats" set UI_ACTION_STATS_ARG=ON
	)
)

if "%ENABLE_TEST%"=="ON" echo unit test enabled.

set _PROJECT_DIR=%~dp0..\..
set PROJECT_DIR=%_PROJECT_DIR:\=/%
set BIN_DIR=%PROJECT_DIR%/bin
set SRC_DIR=%PROJECT_DIR%/src
set OBS_SRC_DIR=%SRC_DIR%/obs-studio
set PRISM_SRC_DIR=%SRC_DIR%/prism-live-studio
set OBS_BUILD_DIR=%OBS_SRC_DIR%/build/%BUILD_TYPE_ARG%
set PRISM_BUILD_DIR=%PRISM_SRC_DIR%/build/%BUILD_TYPE_ARG%
set DEV_OUTPUT_DIR=%BIN_DIR%/prism/windows
set PRISM_COMMENTS=

set QTDIR=%QTDIR:\=/%
set ALL_DEPS=%QTDIR%
set VIRTUALCAM_GUID=A49F51EE-8841-4425-BEC0-85D0C470BBDE

set GENERATOR=Visual Studio 17 2022
set COMPILER=v143
set ARCH=x64,version=10.0.22621

if "%CI_VERSION%"=="" (
	set /p VERSION=<%~dp0version_win.txt
) else (
	set VERSION=%CI_VERSION%
)
set VERSION=%VERSION: =%

if "%BUILD_TYPE_ARG%"=="Debug" (
	set CMAKE_BUILD_TYPE=Debug
) else (
	set CMAKE_BUILD_TYPE=RelWithDebInfo
)

if "%ENABLE_SETUP%"=="" set ENABLE_SETUP=OFF

rem cd %_PROJECT_DIR%
rem git submodule update --init --recursive

cmake -Wno-dev ^
	-S "%SRC_DIR%" ^
 	-B "%PRISM_BUILD_DIR%" ^
	-G "%GENERATOR%" ^
	-A %ARCH% ^
	-T %COMPILER% ^
	-DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
	-DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
	-DRELEASE_CANDIDATE=%VERSION% ^
	-DENABLE_BROWSER=ON ^
	-DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
	-DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
	-DENABLE_SETUP=%ENABLE_SETUP% ^
	-DENABLE_TEST=%ENABLE_TEST% ^
	-DPERFORMANCE_STATS=%PERFORMANCE_STATS_ARG% ^
	-DUI_ACTION_STATS=%UI_ACTION_STATS_ARG%
