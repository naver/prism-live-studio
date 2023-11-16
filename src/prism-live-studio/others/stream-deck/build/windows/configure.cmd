@echo off

setlocal
cd %~dp0

set _PROJECT_DIR=%~dp0..\..
set PROJECT_DIR=%_PROJECT_DIR:\=/%
set SRC_DIR=%PROJECT_DIR%/src/cpp/com.naver.prism.sdPlugin
set BUILD_DIR=%SRC_DIR%/build

set GENERATOR=Visual Studio 17 2022
set COMPILER=v143
set ARCH=x64

cmake -Wno-dev ^
    -S "%SRC_DIR%" ^
 	-B "%BUILD_DIR%" ^
	-G "%GENERATOR%" ^
	-A %ARCH% ^
	-T %COMPILER% ^