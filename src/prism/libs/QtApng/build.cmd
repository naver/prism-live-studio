@echo off

setlocal

set CDIR=%~dp0

set VCVARS64=%1
set BUILD_DIR=%2
set CONFIG=%3
set PRISM_DIR=%4
set BIN_DIR=%5
set ARCH=%6
set QTDIR=%7

if "%CONFIG%"=="RelWithDebInfo" (
	set CONFIG_SRC=Release
) else (
	set CONFIG_SRC=%CONFIG%
)

set OUTPUT_DIR=%BIN_DIR%\%CONFIG_SRC%\%ARCH%

echo VCVARS64=%VCVARS64%
echo BUILD_DIR=%BUILD_DIR%
echo CONFIG=%CONFIG_SRC%
echo PRISM_DIR=%PRISM_DIR%
echo BIN_DIR=%BIN_DIR%
echo ARCH=%ARCH%
echo QTDIR=%QTDIR%
echo OUTPUT_DIR=%OUTPUT_DIR%

chdir %CDIR%
call %VCVARS64%

chdir %BUILD_DIR%
%QTDIR%\bin\qmake.exe %CDIR%qtapng.pro -spec win32-msvc "CONFIG+=debug" "CONFIG+=qml_debug"
nmake.exe

if "%CONFIG_SRC%"=="Debug" (
    if EXIST "%BUILD_DIR%\plugins\imageformats\qapngd.dll" (
		if EXIST "%OUTPUT_DIR%\plugins\imageformats" (
			xcopy /C /I /F /R /Y "%BUILD_DIR%\plugins\imageformats\qapngd.dll" "%OUTPUT_DIR%\plugins\imageformats\"
		) else (
			if EXIST "%OUTPUT_DIR%\imageformats" (
				xcopy /C /I /F /R /Y "%BUILD_DIR%\plugins\imageformats\qapngd.dll" "%OUTPUT_DIR%\imageformats\"
			)
		)
	)
) else (
	if EXIST "%BUILD_DIR%\plugins\imageformats\qapng.dll" (
		if EXIST "%OUTPUT_DIR%\plugins\imageformats" (
			xcopy /C /I /F /R /Y "%BUILD_DIR%\plugins\imageformats\qapng.dll" "%OUTPUT_DIR%\plugins\imageformats\"
		) else (
			if EXIST "%OUTPUT_DIR%\imageformats" (
				xcopy /C /I /F /R /Y "%BUILD_DIR%\plugins\imageformats\qapng.dll" "%OUTPUT_DIR%\imageformats\"
			)
		)
	)
)

chdir %CDIR%
