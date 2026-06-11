@echo off
setlocal

signtool verify /pa %2 1>nul,2>nul
if ERRORLEVEL 1 goto SIGN
goto CHECK_OK

:SIGN
SET SIGN_COUNT=0

:SIGN_RETRY
SET /A SIGN_COUNT=SIGN_COUNT+1
if %SIGN_COUNT% == 3 goto ERROR_SIGN_FAILED

@echo Signing code %2:  %SIGN_COUNT%
echo codesign.exe -k 465 -d sha256 -v %1 -i %2 -o %2
codesign.exe -k 465 -d sha256 -v %1 -i %2 -o %2
if ERRORLEVEL 1 goto SIGN_RETRY
goto OK

:ERROR_SIGN_FAILED
@echo ERROR: Sign code %2 Failed!
exit /b 1

:OK
signtool verify /pa %2 1>nul,2>nul
if ERRORLEVEL 1 goto CHECK_ERROR
goto CHECK_OK

:CHECK_ERROR
@echo ERROR: Verify Sign code %2 Failed!
exit /b 1

:CHECK_OK
OK: Verify Sign code %2 OK!
