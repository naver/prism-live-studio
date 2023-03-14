project(${PLUGIN_NAME})

set(ST_LIBRARIES_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/lib/SoundTouchD_x64.lib)
set(ST_LIBRARIES_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/lib/SoundTouch_x64.lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include)

file(GLOB_RECURSE src_files *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE hdr_files *.h *.hpp *.hxx *.hh)

add_definitions(-D_PRISM_PLUGIN)

add_library(${PLUGIN_NAME} MODULE ${src_files} ${hdr_files})
target_link_libraries(${PLUGIN_NAME}
	liblog
	log
	obs)

set(SoundTouch_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/BPMDetect.h
	${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSampleBuffer.h
	${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSamplePipe.h
	${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/SoundTouch.h
	${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/STTypes.h)

source_group("SoundTouchLib" FILES ${SoundTouch_HEADERS})

target_link_libraries(${PLUGIN_NAME} debug ${ST_LIBRARIES_DEBUG})
target_link_libraries(${PLUGIN_NAME} optimized ${ST_LIBRARIES_RELEASE})

add_obs_dependency(${PLUGIN_NAME})
link_obs(${PLUGIN_NAME})

install_obs_plugin_with_data(${PLUGIN_NAME} data)
copy_prism_plugin_target(${PLUGIN_NAME})
