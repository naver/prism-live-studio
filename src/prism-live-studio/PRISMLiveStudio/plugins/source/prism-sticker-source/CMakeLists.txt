message(STATUS "PRISM Plugin: prism-sticker-source")

project(prism-sticker-source)

add_library(prism-sticker-source MODULE)

add_definitions(-D_PRISM_PLUGIN)
add_definitions(-DQT_NO_VERSION_TAGGING)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat)

target_sources(prism-sticker-source 
	PRIVATE
	prism-sticker-source.hpp
	prism-giphy-sticker-source.hpp
	prism-sticker-source.cpp
	prism-giphy-sticker-source.cpp
	plugin-main.cpp)

target_link_libraries(prism-sticker-source
	PRIVATE
	FFmpeg::avcodec
	FFmpeg::avutil 
	FFmpeg::avformat
	OBS::libobs
	common-libs::utils-api
	common-libs::log)

if(OS_WINDOWS)
	if(MSVC)
		target_link_libraries(prism-sticker-source PRIVATE OBS::w32-pthreads)
	endif()
endif()
prism_module(prism-sticker-source "PRISM Sticker module")
set_target_properties(prism-sticker-source PROPERTIES FOLDER prism/plugins/source)

copy_prism_plugin_binary(prism-sticker-source)
