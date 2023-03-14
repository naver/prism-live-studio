message(STATUS "PRISM Plugin: ${PROJECT_NAME}")

project(prism-stream-deck)

add_definitions(-D_PRISM_PLUGIN)

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

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS network REQUIRED)
find_package(Qt5 COMPONENTS WebSockets REQUIRED)

add_definitions(-D_PRISM_PLUGIN)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Vendor/json/src")

target_link_libraries(prism-stream-deck
	obs-frontend-api
	frontend-api
	Qt5::Widgets
	Qt5::Network
	Qt5::WebSockets)
add_obs_dependency(prism-stream-deck)
link_obs(prism-stream-deck)

copy_prism_plugin_target(prism-stream-deck)

