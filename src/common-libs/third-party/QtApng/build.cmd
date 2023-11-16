@echo off

setlocal

set CDIR=%~dp0

set VCVARS64=%1
set BUILD_DIR=%2
set CONFIG=%3
set PRISM_SRC_DIR=%4
set DEV_OUTPUT_DIR=%5
set ARCH=%6
set QTDIR=%7

if "%CONFIG%"=="Debug" (
	set CONFIG_SRC=Debug
) else (
	set CONFIG_SRC=Release
)

set OUTPUT_DIR=%DEV_OUTPUT_DIR%\%CONFIG_SRC%\bin\64bit

echo VCVARS64=%VCVARS64%
echo BUILD_DIR=%BUILD_DIR%
echo CONFIG=%CONFIG_SRC%
echo PRISM_SRC_DIR=%PRISM_SRC_DIR%
echo DEV_OUTPUT_DIR=%DEV_OUTPUT_DIR%
echo ARCH=%ARCH%
echo QTDIR=%QTDIR%
echo OUTPUT_DIR=%OUTPUT_DIR%

chdir "%CDIR%"
call "%VCVARS64%"

chdir "%BUILD_DIR%\%CONFIG%"

if "%CONFIG_SRC%"=="Debug" (
	"%QTDIR%\bin\qmake.exe" "%CDIR%qtapng.pro" -spec win32-msvc "CONFIG+=debug" "CONFIG+=qml_debug"
	nmake.exe

    if EXIST "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapngd.dll" (
		if EXIST "%OUTPUT_DIR%\plugins\imageformats" (
			xcopy /C /I /F /R /Y "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapngd.dll" "%OUTPUT_DIR%\plugins\imageformats\"
		) else (
			if EXIST "%OUTPUT_DIR%\imageformats" (
				xcopy /C /I /F /R /Y "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapngd.dll" "%OUTPUT_DIR%\imageformats\"
			)
		)
	)
) else (
	"%QTDIR%\bin\qmake.exe" "%CDIR%qtapng.pro" -spec win32-msvc "CONFIG+=qtquickcompiler"
	nmake.exe

	if EXIST "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapng.dll" (
		if EXIST "%OUTPUT_DIR%\plugins\imageformats" (
			xcopy /C /I /F /R /Y "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapng.dll" "%OUTPUT_DIR%\plugins\imageformats\"
		) else (
			if EXIST "%OUTPUT_DIR%\imageformats" (
				xcopy /C /I /F /R /Y "%BUILD_DIR%\%CONFIG%\plugins\imageformats\qapng.dll" "%OUTPUT_DIR%\imageformats\"
			)
		)
	)
)

chdir %CDIR%
