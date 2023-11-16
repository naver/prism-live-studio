@echo off

setlocal
cd %~dp0
call common_values.cmd
rem cd %_PROJECT_DIR%
rem git submodule update --init --recursive

call powershell %_PROJECT_DIR%\src\obs-studio\CI\windows\01_install_dependencies.ps1

cmake -Wno-dev ^
	-S "%SRC_DIR%" ^
 	-B "%PRISM_BUILD_DIR%" ^
	-G "%GENERATOR%" ^
	-A %ARCH% ^
	-T %COMPILER% ^
	-DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
	-DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
	-DRELEASE_CANDIDATE=%VERSION% ^
	-DCOPY_DEPENDENCIES=ON ^
	-DBUILD_CAPTIONS=ON ^
	-DCOMPILE_D3D12_HOOK=ON ^
	-DENABLE_BROWSER=ON ^
	-DCEF_ROOT_DIR=%CEF_ROOT_DIR% ^
	-DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
	-DENABLE_UI=ON ^
    -DVLC_PATH=%VLC_DIR% ^
    -DENABLE_VLC=ON ^
	-DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
	-DENABLE_SETUP=%ENABLE_SETUP%
