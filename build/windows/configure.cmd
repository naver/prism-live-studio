@echo off

setlocal

cd %~dp0

if "%1"=="Debug" (
	set QTDIR=%QTDIR_Debug%
	set BUILD_TYPE_ARG=Debug
)
if "%1"=="Release" (
	set QTDIR=%QTDIR_RelWithDebInfo%
	set BUILD_TYPE_ARG=Release
)
if "%1"=="RelWithDebInfo" (
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

if "%ENABLE_TEST%"=="ON" echo unit test enabled.

call common_values.cmd
if ERRORLEVEL 1 exit /b 1
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
	-DENABLE_TEST=%ENABLE_TEST%
