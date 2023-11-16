@echo off

setlocal

set OBS_ENV=%1

call %OBS_ENV%

rem call powershell %OBS_DIR%\CI\windows\01_install_dependencies.ps1

echo obs configure.

echo %OBS_VERSION% | findstr beta >nul && (
	cmake -Wno-dev ^
        -S "%OBS_SRC_DIR%" ^
        -B "%OBS_BUILD_DIR%" ^
        -G "%GENERATOR%" ^
        -A x64 ^
        -T v143 ^
        -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
        -DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
        -DBETA=%OBS_VERSION% ^
        -DCOPIED_DEPENDENCIES=OFF ^
        -DCOPY_DEPENDENCIES=ON ^
        -DBUILD_CAPTIONS=ON ^
        -DCOMPILE_D3D12_HOOK=ON ^
        -DBUILD_BROWSER=ON ^
        -DCEF_ROOT_DIR=%CEF_ROOT_DIR% ^
        -DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
        -DENABLE_UI=ON ^
        -DVLC_PATH=%VLC_DIR% ^
        -DENABLE_VLC=ON ^
        -DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
        -DQT_VERSION=6

    cmake -Wno-dev ^
        -S "%OBS_SRC_DIR%" ^
        -B "%OBS_BUILD_DIR%"/x86 ^
        -G "%GENERATOR%" ^
        -A Win32 ^
        -T v143 ^
        -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
        -DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
        -DBETA=%OBS_VERSION% ^
        -DCOPIED_DEPENDENCIES=OFF ^
        -DCOPY_DEPENDENCIES=ON ^
        -DBUILD_CAPTIONS=ON ^
        -DCOMPILE_D3D12_HOOK=ON ^
        -DBUILD_BROWSER=ON ^
        -DCEF_ROOT_DIR=%CEF_ROOT_DIR% ^
        -DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
        -DENABLE_UI=ON ^
        -DVLC_PATH=%VLC_DIR% ^
        -DENABLE_VLC=ON ^
        -DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
        -DQT_VERSION=6
) || (
	cmake -Wno-dev ^
        -S "%OBS_SRC_DIR%" ^
        -B "%OBS_BUILD_DIR%" ^
        -G "%GENERATOR%" ^
        -A x64 ^
        -T v143 ^
        -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
        -DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
        -DRELEASE_CANDIDATE=%OBS_VERSION% ^
        -DCOPIED_DEPENDENCIES=OFF ^
        -DCOPY_DEPENDENCIES=ON ^
        -DBUILD_CAPTIONS=ON ^
        -DCOMPILE_D3D12_HOOK=ON ^
        -DBUILD_BROWSER=ON ^
        -DCEF_ROOT_DIR=%CEF_ROOT_DIR% ^
        -DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
        -DENABLE_UI=ON ^
        -DVLC_PATH=%VLC_DIR% ^
        -DENABLE_VLC=ON ^
        -DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
        -DQT_VERSION=6

    cmake -Wno-dev ^
        -S "%OBS_SRC_DIR%" ^
        -B "%OBS_BUILD_DIR%"/x86 ^
        -G "%GENERATOR%" ^
        -A Win32 ^
        -T v143 ^
        -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
        -DCMAKE_PREFIX_PATH="%ALL_DEPS%" ^
        -DRELEASE_CANDIDATE=%OBS_VERSION% ^
        -DCOPIED_DEPENDENCIES=OFF ^
        -DCOPY_DEPENDENCIES=ON ^
        -DBUILD_CAPTIONS=ON ^
        -DCOMPILE_D3D12_HOOK=ON ^
        -DBUILD_BROWSER=ON ^
        -DCEF_ROOT_DIR=%CEF_ROOT_DIR% ^
        -DVIRTUALCAM_GUID="%VIRTUALCAM_GUID%" ^
        -DENABLE_UI=ON ^
        -DVLC_PATH=%VLC_DIR% ^
        -DENABLE_VLC=ON ^
        -DCMAKE_POLICY_DEFAULT_CMP0048=NEW ^
        -DQT_VERSION=6
)
