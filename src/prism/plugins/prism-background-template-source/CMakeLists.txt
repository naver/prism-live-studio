message(STATUS "PRISM Plugin: ${PLUGIN_NAME}")

project(${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D_PRISM_PLUGIN)

if(MSVC)
	set(prism_background_template_source_PLATFORM_DEPS
		w32-pthreads)
endif()

set(prism_background_template_source_HEADERS
	prism_background_template_source.hpp)

set(prism_background_template_source_SOURCES
	prism_background_template_source.cpp
	plugin-main.cpp)

add_library(${PLUGIN_NAME} MODULE
	${prism_background_template_source_SOURCES}
	${prism_background_template_source_HEADERS})
target_link_libraries(${PLUGIN_NAME}
	liblog
	log
        ${prism_background_template_source_PLATFORM_DEPS})

add_obs_dependency(${PLUGIN_NAME})
link_obs(${PLUGIN_NAME})

install_obs_plugin_with_data(${PLUGIN_NAME} data)
copy_prism_plugin_target(${PLUGIN_NAME})
