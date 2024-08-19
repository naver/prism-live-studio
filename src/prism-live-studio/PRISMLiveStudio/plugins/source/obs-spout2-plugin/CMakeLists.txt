if (NOT WIN32)
    return()
endif()
cmake_minimum_required(VERSION 3.5)
project(win-spout)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/plugins/win-spout/external")

set(LIBOBS_LIB OBS::libobs)
set(OBS_FRONTEND_LIB OBS::frontend-api)
set(PTHREAD_LIB OBS::w32-pthreads)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

if(MSVC)
    include_directories(deps/Spout2/SPOUTSDK/SpoutLibrary)
    include_directories(deps/Spout2/SPOUTSDK/SpoutDirectX/SpoutDX)
    link_directories(deps/Spout2/BUILD/Binaries/x64)
endif()

set(win-spout_HEADERS
    source/win-spout.h
    source/win-spout-config.h
    source/ui/win-spout-output-settings.h)

set(win-spout_SOURCES
    source/win-spout.cpp
    source/win-spout-source.cpp
    source/win-spout-output.cpp
    source/win-spout-filter.cpp
    source/win-spout-config.cpp
    source/ui/win-spout-output-settings.cpp)

add_library(win-spout MODULE
    ${win-spout_SOURCES}
    ${win-spout_HEADERS})

set(SPOUTDX_LIB "${CMAKE_CURRENT_SOURCE_DIR}/deps/Spout2/BUILD/Binaries/x64/SpoutDX.lib")

target_link_libraries(win-spout
    ${OBS_FRONTEND_LIB}
    ${PTHREAD_LIB}
    ${LIBOBS_LIB}
    ${SPOUTDX_LIB}
    OBS::w32-pthreads
    Qt6::Core
    Qt6::Widgets
    common-libs::ui
    prism::frontend-api
    )

function(copy_spout_file)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_src_suffix "x64")
    else()
        set(_src_suffix "win32")
    endif()
    copy_prism_plugin_dep(win-spout Spout ${CMAKE_CURRENT_SOURCE_DIR}/deps/Spout2/BUILD/Binaries/${_src_suffix}/ prism-plugins/64bit)
    copy_prism_plugin_dep(win-spout SpoutLibrary ${CMAKE_CURRENT_SOURCE_DIR}/deps/Spout2/BUILD/Binaries/${_src_suffix}/ prism-plugins/64bit)
    copy_prism_plugin_dep(win-spout SpoutDX ${CMAKE_CURRENT_SOURCE_DIR}/deps/Spout2/BUILD/Binaries/${_src_suffix}/ prism-plugins/64bit)
endfunction()

prism_module(win-spout "PRISM spout module")
set_target_properties(win-spout PROPERTIES FOLDER "prism/plugins/source")

copy_spout_file()
copy_prism_plugin_binary(win-spout)
