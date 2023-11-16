if (NOT WIN32)
	return()
endif()

message(STATUS "PRISM Plugin: prism-monitor-capture")

project(prism-monitor-capture)

file(GLOB_RECURSE src_files *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE hdr_files *.h *.hpp *.hxx *.hh)

add_definitions(-D_PRISM_PLUGIN)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_library(${PROJECT_NAME} MODULE ${src_files} ${hdr_files})
target_link_libraries(${PROJECT_NAME}
	common-libs::log
	common-libs::utils-api
	OBS::libobs
	gdiplus
	d3d9
	d3d11
	dxgi)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "prism/plugins/source")
copy_prism_plugin_binary(${PROJECT_NAME})
