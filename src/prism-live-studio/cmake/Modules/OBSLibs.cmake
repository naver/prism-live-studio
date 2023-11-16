if(OS_WINDOWS)
    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/obs.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/obs.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/obs.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/obs.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.dll")

    add_library(OBS::frontend-api SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::frontend-api PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api")
    set_target_properties(
        OBS::frontend-api PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dll")

	add_library(OBS::w32-pthreads SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::w32-pthreads PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/w32-pthreads")
    set_target_properties(
        OBS::w32-pthreads PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.dll")

    add_library(OBS::blake2 STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src")
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/blake2/Debug/blake2.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/blake2/Release/blake2.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/blake2/RelWithDebInfo/blake2.lib")

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Debug/obs-scripting.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Release/obs-scripting.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/RelWithDebInfo/obs-scripting.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Debug/obs-scripting.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Release/obs-scripting.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/RelWithDebInfo/obs-scripting.dll")
elseif(OS_MACOS)
    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/libobs.framework/Versions/A/libobs"
        IMPORTED_SONAME "@rpath/llibobs.framework/Versions/A/libobs"
    )
    
    add_library(OBS::frontend-api SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::frontend-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api"
    )
    set_target_properties(
        OBS::frontend-api PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/libobs-frontend-api.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/libobs-frontend-api.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/libobs-frontend-api.dylib"
        IMPORTED_SONAME "@rpath/libobs-frontend-api.1.dylib"
    )

    add_library(OBS::blake2 SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src"
    )
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/blake2/Debug/libblake2.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/blake2/Release/libblake2.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/blake2/RelWithDebInfo/libblake2.a"
    )

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Debug/libobs-scripting.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/Release/libobs-scripting.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/obs-scripting/RelWithDebInfo/libobs-scripting.dylib"
        IMPORTED_SONAME "@rpath/libobs-scripting.1.dylib"
    )

endif()

