message(STATUS "PRISM Plugin: ${PLUGIN_NAME}")

project(${PLUGIN_NAME})

if(MSVC)
	set(prism-sticker-source_PLATFORM_DEPS
		w32-pthreads)
endif()

find_package(FFmpeg REQUIRED COMPONENTS avutil avcodec avformat)
include_directories(${FFMPEG_INCLUDE_DIRS})

add_definitions(-D_PRISM_PLUGIN)

set(prism-sticker-source_HEADERS
	prism-sticker-source.hpp
	prism-giphy-sticker-source.hpp)

set(prism-sticker-source_SOURCES
	prism-sticker-source.cpp
	prism-giphy-sticker-source.cpp
	plugin-main.cpp)

add_library(${PLUGIN_NAME} MODULE

	${prism-sticker-source_SOURCES}
	${prism-sticker-source_HEADERS})
target_link_libraries(${PLUGIN_NAME}
	liblog
	log
        ${prism-sticker-source_PLATFORM_DEPS})

add_obs_dependency(${PLUGIN_NAME})
link_obs(${PLUGIN_NAME})

install_obs_plugin_with_data(${PLUGIN_NAME} data)
copy_prism_plugin_target(${PLUGIN_NAME})
