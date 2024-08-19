project(qapng)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(Qt6 COMPONENTS Gui REQUIRED)

file(GLOB files *.h *.cpp)
add_library(qapng SHARED ${files})
set_target_properties(qapng PROPERTIES
	OUTPUT_NAME_DEBUG qapngd
	OUTPUT_NAME_RELEASE qapng
	OUTPUT_NAME_RELWITHDEBINFO qapng
	OUTPUT_NAME_MINSIZEREL qapng
	FOLDER common-libs/third-party/qapng)
target_link_libraries(qapng qtapng_zlib qtapng_png Qt6::Core Qt6::Gui)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    setup_qt_third_plugin(qapng)
endif()

function(copy_third_party_apng target)
    if(OS_WINDOWS)
        add_dependencies(${target} qapng)
        add_custom_command(TARGET ${target}
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DCONFIG=$<CONFIGURATION>"
                "-Dqapng_BINARY_DIR=${qapng_BINARY_DIR}"
                "-DDEV_OUTPUT_DIR=$ENV{DEV_OUTPUT_DIR}"
                "-DARCH=$ENV{ARCH}"
                "-DTARGET=qapng"
                "-DOS_WINDOWS=${OS_WINDOWS}"
                "-DOS_MACOS=${OS_MACOS}"
                -P "${QtApng_SOURCE_DIR}/copy.cmake"
            VERBATIM)
    endif()
endfunction()
