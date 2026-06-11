set(PLUGIN_NAME "prism-timer-source")
message(STATUS "PRISM Plugin: ${PLUGIN_NAME}")

project(${PLUGIN_NAME})

set(CMAKE_AUTORCC True)
find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(prism-timer-source_HEADERS
	prism-timer-source.hpp)

set(prism-timer-source_SOURCES
	prism-timer-source.cpp
	plugin-main.cpp)

add_library(${PLUGIN_NAME} MODULE
	${prism-timer-source_SOURCES}
	${prism-timer-source_HEADERS})

target_link_libraries(${PLUGIN_NAME}
	frontend-api
    ${prism-timer-source_PLATFORM_DEPS}
	Qt6::Core
	common-libs::utils-api 
	common-libs::log
	prism::frontend-api)

target_sources(${PLUGIN_NAME} PRIVATE timer_source.qrc)

prism_module(${PLUGIN_NAME} "PRISM timer clock module")
set_target_properties(${PLUGIN_NAME} PROPERTIES FOLDER "prism/plugins/source")

if(OS_MACOS)
setup_plugin_target(${PLUGIN_NAME})
elseif(OS_WINDOWS)
copy_prism_plugin_binary(${PLUGIN_NAME})
endif()
