@echo off


set _PROJECT_DIR=%~dp0..\..
set PROJECT_DIR=%_PROJECT_DIR:\=/%
set BIN_DIR=%PROJECT_DIR%/bin
set SRC_DIR=%PROJECT_DIR%/src

set OBS_SRC_DIR=%SRC_DIR%/obs-studio
set PRISM_SRC_DIR=%SRC_DIR%/prism-live-studio
set OBS_BUILD_DIR=%OBS_SRC_DIR%/build
set PRISM_BUILD_DIR=%PRISM_SRC_DIR%/build

for /F "delims=; tokens=1,2,3" %%i in ('call powershell %_PROJECT_DIR%\build\windows\get_deps.ps1') do (
	set WindowsDepsVersion=%%i
	set WindowsVlcVersion=%%j
	set WindowsCefVersion=%%k
)
echo WindowsDepsVersion=%WindowsDepsVersion%
echo WindowsVlcVersion=%WindowsVlcVersion%
echo WindowsCefVersion=%WindowsCefVersion%

set CURRENT_SCRIPT_DIR=%PROJECT_DIR%/build/windows
set zipAppPath=%CURRENT_SCRIPT_DIR%\Package\SetupScripts\app.7z
set SETUP_OUTPUT_DIR=%PROJECT_DIR%/build/windows/Package/SetupScripts/prism/QtInstaller
set InstallOutputFilePath=%CURRENT_SCRIPT_DIR%/Package/Output/PRISMLiveStudioSetup64.exe
if "%ENABLE_SETUP%"=="" set ENABLE_SETUP=OFF
set CEF_ROOT_DIR=%SRC_DIR%/obs-build-dependencies/cef_binary_%WindowsCefVersion%_windows_x64
set VLC_DIR=%SRC_DIR%/obs-build-dependencies/vlc-%WindowsVlcVersion%
set DEPS_DIR=%SRC_DIR%/obs-build-dependencies/windows-deps-%WindowsDepsVersion%-x64
if not "%QT631%"=="" set QTDIR=%QT631%
set QTDIR=%QTDIR:\=/%
set VIRTUALCAM_GUID=A49F51EE-8841-4425-BEC0-85D0C470BBDE
set ALL_DEPS=%QTDIR%;%DEPS_DIR%

rem for signe
set ExcludesReg="\W(Qt)|(q).+dll"

set GENERATOR=Visual Studio 17 2022
set COMPILER=v143
set ARCH=x64

if "%CI_VERSION%"=="" (
	set VERSION=4.0.0.0
) else (
	set VERSION=%CI_VERSION%
)
if "%BUILD_BRANCH%"=="" set BUILD_BRANCH=develop
set PRISM_COMMENTS=

if not exist BUILD_RANGE_ARG set BUILD_RANGE_ARG=rebuild

if "%BUILD_RANGE_ARG%"=="" (
 	set BUILD_RANGE=rebuild
 ) else (
 	set BUILD_RANGE=%BUILD_RANGE_ARG%
)

if "%BUILD_RANGE%"=="rebuild" set CLEAN_FIRST=--clean-first

if "%BUILD_TYPE_ARG%"=="Debug" (
	set BUILD_TYPE=Debug
	set CMAKE_BUILD_TYPE=Debug
) else (
	set BUILD_TYPE=Release
	set CMAKE_BUILD_TYPE=RelWithDebInfo
)

set OBS_VERSION=29.0.2.0
set DEV_OUTPUT_DIR=%BIN_DIR%/prism/windows
set OUTPUT_DIR=%DEV_OUTPUT_DIR%/%BUILD_TYPE%

echo PROJECT_DIR=%PROJECT_DIR%
echo BIN_DIR=%BIN_DIR%
echo SRC_DIR=%SRC_DIR%
echo OBS_SRC_DIR=%OBS_SRC_DIR%
echo PRISM_SRC_DIR=%PRISM_SRC_DIR%
echo OBS_BUILD_DIR=%OBS_BUILD_DIR%
echo PRISM_BUILD_DIR=%PRISM_BUILD_DIR%
echo CEF_ROOT_DIR=%CEF_ROOT_DIR%
echo VLC_DIR=%VLC_DIR%
echo DEPS_DIR=%DEPS_DIR%
echo QTDIR=%QTDIR%
echo VIRTUALCAM_GUID=%VIRTUALCAM_GUID%
echo ALL_DEPS=%ALL_DEPS%
echo GENERATOR=%GENERATOR%
echo COMPILER=%COMPILER%
echo ARCH=%ARCH%
echo VERSION=%VERSION%
echo PRISM_COMMENTS=%PRISM_COMMENTS%
echo OUTPUT_DIR=%OUTPUT_DIR%
echo SETUP_OUTPUT_DIR=%SETUP_OUTPUT_DIR%
echo ENABLE_SETUP=%ENABLE_SETUP%
echo OBS_VERSION=%OBS_VERSION%
