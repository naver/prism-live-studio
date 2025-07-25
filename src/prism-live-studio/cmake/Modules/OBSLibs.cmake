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
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/libobs/Debug/obs.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/libobs/Release/obs.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/libobs/RelWithDebInfo/obs.dll")

    add_library(OBS::frontend-api SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::frontend-api PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api")
    set_target_properties(
        OBS::frontend-api PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dll")

	add_library(OBS::w32-pthreads SHARED IMPORTED GLOBAL)
	set_target_properties(
		OBS::w32-pthreads PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/w32-pthreads")
    set_target_properties(
        OBS::w32-pthreads PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Debug/w32-pthreads.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/Release/w32-pthreads.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/w32-pthreads/RelWithDebInfo/w32-pthreads.dll")

    add_library(OBS::blake2 STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src")
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/Debug/blake2.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/Release/blake2.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/RelWithDebInfo/blake2.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/blake2/blake2.dir/RelWithDebInfo/blake2.lib")

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_IMPLIB_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.lib"
        IMPORTED_IMPLIB_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.lib"
        IMPORTED_IMPLIB_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.lib"
        IMPORTED_IMPLIB_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.lib"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.dll"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.dll"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dll"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dll")

    add_library(OBS::caption STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::caption PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/libcaption")
    set_target_properties(
        OBS::caption PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/libcaption/Debug/caption.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/libcaption/Release/caption.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/caption.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/caption.lib")

    add_library(OBS::bpm STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::bpm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/bpm")
    set_target_properties(
        OBS::bpm PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/Debug/bpm.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/Release/bpm.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/RelWithDebInfo/bpm.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/bpm/bpm.dir/RelWithDebInfo/bpm.lib")

    add_library(OBS::aja-support STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::aja-support PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/plugins/aja")
    set_target_properties(
        OBS::aja-support PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/Debug/aja-support.lib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/Release/aja-support.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/RelWithDebInfo/aja-support.lib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/plugins/aja/aja-support.dir/RelWithDebInfo/aja-support.lib")
elseif(OS_MACOS)
    add_library(OBS::libobs SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::libobs PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/libobs;$ENV{OBS_BUILD_DIR}/config")
    set_target_properties(
        OBS::libobs PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/Debug/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/Release/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/RelWithDebInfo/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/RelWithDebInfo/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
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
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Debug/obs-frontend-api.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/Release/obs-frontend-api.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dylib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/UI/obs-frontend-api/RelWithDebInfo/obs-frontend-api.dylib"
        IMPORTED_SONAME "@rpath/obs-frontend-api.dylib"
    )

    add_library(OBS::blake2 SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::blake2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/blake2/src"
    )
    set_target_properties(
        OBS::blake2 PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/blake2.build/Debug/libblake2.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/blake2.build/Release/libblake2.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/blake2.build/RelWithDebInfo/libblake2.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/blake2.build/RelWithDebInfo/libblake2.a"
    )

    add_library(obs-browser-panels INTERFACE IMPORTED GLOBAL)
    add_library(OBS::browser-panels ALIAS obs-browser-panels)
    target_include_directories(
        obs-browser-panels INTERFACE "$ENV{OBS_SRC_DIR}/plugins/obs-browser/panel")

    add_library(OBS::scripting SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::scripting PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/obs-scripting")
    set_target_properties(
        OBS::scripting PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Debug/obs-scripting.dylib"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/Release/obs-scripting.dylib"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dylib"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/shared/obs-scripting/RelWithDebInfo/obs-scripting.dylib"
        IMPORTED_SONAME "@rpath/obs-scripting.dylib"
    )

    add_library(OBS::caption SHARED IMPORTED GLOBAL)
    set_target_properties(
        OBS::caption PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/deps/libcaption"
    )
    set_target_properties(
        OBS::caption PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo"
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/deps/libcaption/Debug/libcaption.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/deps/libcaption/Release/libcaption.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/libcaption.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/deps/libcaption/RelWithDebInfo/libcaption.a"
    )

    add_library(OBS::bpm STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::bpm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/shared/bpm")
    set_target_properties(
        OBS::bpm PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/bpm.build/Debug/libbpm.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/bpm.build/Release/libbpm.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/bpm.build/RelWithDebInfo/libbpm.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/bpm.build/RelWithDebInfo/libbpm.a")

    add_library(OBS::aja-support STATIC IMPORTED GLOBAL)
    set_target_properties(
        OBS::aja-support PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{OBS_SRC_DIR}/plugins/aja")
    set_target_properties(
        OBS::aja-support PROPERTIES
        IMPORTED_LOCATION_DEBUG "$ENV{OBS_BUILD_DIR}/build/aja-support.build/Debug/libaja-support.a"
        IMPORTED_LOCATION_RELEASE "$ENV{OBS_BUILD_DIR}/build/aja-support.build/Release/libaja-support.a"
        IMPORTED_LOCATION_RELWITHDEBINFO "$ENV{OBS_BUILD_DIR}/build/aja-support.build/RelWithDebInfo/libaja-support.a"
        IMPORTED_LOCATION_MINSIZEREL "$ENV{OBS_BUILD_DIR}/build/aja-support.build/RelWithDebInfo/libaja-support.a")
endif()
