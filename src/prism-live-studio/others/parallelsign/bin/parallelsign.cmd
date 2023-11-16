@echo off
setlocal

set Path=%PROJECT_DIR%\build\windows\parallelsign;%PROJECT_DIR%\build\windows;%Path%
set PARALLEL_COUNT=2

parallelsign.exe
if ERRORLEVEL 1 goto ERROR

goto OK
:ERROR
exit /b 1

:OK
