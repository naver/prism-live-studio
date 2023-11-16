cmake_minimum_required(VERSION 3.20)
project(win-capture-audio VERSION 2.2.2)

include_directories("$ENV{OBS_DIR}/deps/wil/include")

#//PRISM/WangShaohui/20220310/none/integrate plugin
set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std:c++latest")
set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std:c++latest")

set(PLUGIN_AUTHOR "bozbez")
set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_NAME "64bit")
else()
    set(ARCH_NAME "32bit")
endif()

configure_file(
    installer/installer.iss.in
    ../installer/installer.generated.iss
)

execute_process(
    COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    src/plugin-macros.hpp.in
    ${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.hpp
)

#//PRISM/WangShaohui/20220310/none/integrate plugin
#find_package(LibObs REQUIRED)

#//PRISM/WangShaohui/20220310/none/integrate plugin
set(win-capture-audio_HEADERS
	src/audio-capture.hpp
	src/audio-capture-helper.hpp
	src/audio-capture-helper-manager.hpp
	src/common.hpp
	src/format-conversion.hpp
	src/mixer.hpp
	src/session-monitor.hpp)

set(win-capture-audio_SOURCES
    src/plugin.cpp
    src/audio-capture.cpp
    src/audio-capture-helper.cpp
    src/session-monitor.cpp
    src/mixer.cpp)

add_library(win-capture-audio MODULE ${win-capture-audio_SOURCES} ${win-capture-audio_HEADERS})
target_link_libraries(win-capture-audio dwmapi psapi ksuser mmdevapi mfplat)
add_obs_dependency(win-capture-audio)
link_obs(win-capture-audio)

set_property(TARGET win-capture-audio PROPERTY CXX_STANDARD 20)
set_target_properties(win-capture-audio PROPERTIES FOLDER plugins/source)
target_include_directories(win-capture-audio PUBLIC ${CMAKE_SOURCE_DIR}/deps/wil/include)

#//PRISM/WangShaohui/20220310/none/integrate plugin
#install_obs_plugin_with_data(win-capture-audio data)
#copy_prism_plugin_target(win-capture-audio)
copy_prism_lab_plugin_target(win-capture-audio)
