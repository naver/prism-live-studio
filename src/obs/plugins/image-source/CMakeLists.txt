project(image-source)

if(MSVC)
	set(image-source_PLATFORM_DEPS
		w32-pthreads)
endif()

find_package(FFmpeg REQUIRED COMPONENTS avutil avcodec avformat)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(image-source_SOURCES
	image-source.c
	color-source.c
	obs-slideshow.c)

add_library(image-source MODULE
	${image-source_SOURCES})
target_link_libraries(image-source
	libobs
	${image-source_PLATFORM_DEPS})

install_obs_plugin_with_data(image-source data)
