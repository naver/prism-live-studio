@echo off

setlocal

set OBS_DIR=%1
set PRISM_DIR=%2
set OUTPUT_BIN_DIR=%3
set BUILD_TYPE=%4

echo Copy libs.

set OBS_BUILD_DIR=%OBS_DIR%\build
set PRISM_BUILD_DIR=%PRISM_DIR%\build

set PRISM_BUILD_ARCH_DIR=%PRISM_BUILD_DIR%
if NOT "%MULTI_ARCH%"=="" set PRISM_BUILD_ARCH_DIR=%PRISM_BUILD_ARCH_DIR%\%MULTI_ARCH%

if "%BUILD_TYPE%"=="Debug" (
    if EXIST "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapngd.dll" (
        if EXIST "%OUTPUT_BIN_DIR%\plugins\imageformats" (
            xcopy /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapngd.dll" "%OUTPUT_BIN_DIR%\plugins\imageformats\"
        ) else (
            if EXIST "%OUTPUT_BIN_DIR%\imageformats" (
                xcopy /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapngd.dll" "%OUTPUT_BIN_DIR%\imageformats\"
            )
        )
    )
) else (
    if EXIST "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapng.dll" (
        if EXIST "%OUTPUT_BIN_DIR%\plugins\imageformats" (
            xcopy /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapng.dll" "%OUTPUT_BIN_DIR%\plugins\imageformats\"
        ) else (
            if EXIST "%OUTPUT_BIN_DIR%\imageformats" (
                xcopy /C /I /F /R /Y "%PRISM_BUILD_ARCH_DIR%\libs\QtApng\plugins\imageformats\qapng.dll" "%OUTPUT_BIN_DIR%\imageformats\"
            )
        )
    )
)
