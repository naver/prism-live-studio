message(STATUS "PRISM Plugin: ${PROJECT_NAME}")

project(prism-stream-deck)

add_definitions(-D_PRISM_PLUGIN)
set(CMAKE_AUTOMOC ON)

if(MSVC)
	set(prism-stream-deck_PLATFORM_DEPS
		w32-pthreads)
endif()

set(prism-stream-deck_HEADERS
	actionhelp.h
	SourceInfo.h
	streamdeckplugin_module.h
	obspluginstreamdeck_global.h
	JSONUtils.h
	)
set(prism-stream-deck_SOURCES
	actionhelp.cpp
	streamdeckplugin_module.cpp
	)

add_library(prism-stream-deck MODULE
	${prism-stream-deck_HEADERS}
	${prism-stream-deck_SOURCES}
	${prism_stream_deck_UI_HEADERS})

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

add_definitions(-D_PRISM_PLUGIN)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Vendor/json/src")

target_link_libraries(prism-stream-deck
	PRIVATE prism::frontend-api
	common-libs::log
	OBS::libobs
    OBS::frontend-api
	Qt6::Core
	Qt6::WebSockets)
set_target_properties(prism-stream-deck PROPERTIES FOLDER prism/plugins/source)

copy_prism_plugin_binary(prism-stream-deck)



