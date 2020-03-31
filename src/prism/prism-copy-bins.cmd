@echo off

setlocal

set OBS_DIR=%1
set PRISM_DIR=%2
set OUTPUT_BIN_DIR=%3
set BUILD_TYPE=%4

set OBS_BUILD_DIR=%OBS_DIR%\build
set PRISM_BUILD_DIR=%PRISM_DIR%\build

set PRISM_BUILD_ARCH_DIR=%PRISM_BUILD_DIR%
if NOT "%MULTI_ARCH%"=="" set PRISM_BUILD_ARCH_DIR=%PRISM_BUILD_ARCH_DIR%\%MULTI_ARCH%

if "%ARCH%"=="Win32" (
  set SUFFIX=32bit
) else (
  set SUFFIX=64bit
)

if NOT EXIST "%OUTPUT_BIN_DIR%\data\prism-plugins" mkdir "%OUTPUT_BIN_DIR%\data\prism-plugins"
if NOT EXIST "%OUTPUT_BIN_DIR%\data\prism-studio" mkdir "%OUTPUT_BIN_DIR%\data\prism-studio"
if NOT EXIST "%OUTPUT_BIN_DIR%\prism-plugins" mkdir "%OUTPUT_BIN_DIR%\prism-plugins"

chdir %PRISM_DIR%
if EXIST "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\bin\%SUFFIX%" xcopy /E /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\bin\%SUFFIX%" "%OUTPUT_BIN_DIR%"
if EXIST "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\data\obs-plugins" xcopy /E /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\data\obs-plugins" "%OUTPUT_BIN_DIR%\data\prism-plugins"
if EXIST "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\data\prism-studio" xcopy /E /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\data\prism-studio" "%OUTPUT_BIN_DIR%\data\prism-studio"
if EXIST "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\obs-plugins\%SUFFIX%" xcopy /E /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\rundir\%BUILD_TYPE%\obs-plugins\%SUFFIX%" "%OUTPUT_BIN_DIR%\prism-plugins"
