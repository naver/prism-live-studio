
message(STATUS "PRISM Plugin: ${PLUGIN_NAME}")

project(${PLUGIN_NAME})

set(CMAKE_AUTORCC True)
find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

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
	Qt5::Core
	Qt5::Widgets)


add_obs_dependency(${PLUGIN_NAME})
link_obs(${PLUGIN_NAME})

install_obs_plugin_with_data(${PLUGIN_NAME} data)
copy_prism_plugin_target(${PLUGIN_NAME})
