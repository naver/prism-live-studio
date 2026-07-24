set(PLUGIN_NAME "prism-lens-v2")
message(STATUS "PRISM Plugin: ${PLUGIN_NAME}")

project(${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

file(GLOB_RECURSE SOURCES_main *.c *.cpp *.h *.hpp *.hxx)
add_library(${PLUGIN_NAME} MODULE ${SOURCES_main})

target_include_directories(${PLUGIN_NAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/common
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PLUGIN_NAME}
	frontend-api
	common-libs::log
	prism::frontend-api)

prism_module(${PLUGIN_NAME} "PRISM_lens_v2")
set_target_properties(${PLUGIN_NAME} PROPERTIES FOLDER "prism/plugins/source")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES_main})

copy_prism_plugin_binary(${PLUGIN_NAME})