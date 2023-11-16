message(STATUS "PRISM Plugin: prism-sound-touch")

project(prism-sound-touch)

add_library(prism-sound-touch MODULE)

if(OS_WINDOWS)
	set(ST_LIBRARIES_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/windows/SoundTouchD_x64.lib)
	set(ST_LIBRARIES_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/windows/SoundTouch_x64.lib)
elseif(OS_MACOS)
	set(ST_LIBRARIES_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/mac/libSoundTouchD.a)
	set(ST_LIBRARIES_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/mac/libSoundTouch.a)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/soundtouch/include)

file(GLOB_RECURSE src_files *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE hdr_files *.h *.hpp *.hxx *.hh)

add_definitions(-D_PRISM_PLUGIN)

target_sources(
	prism-sound-touch 
	PRIVATE
		${src_files}
		${hdr_files})
target_link_libraries(prism-sound-touch OBS::libobs common-libs::log prism::frontend-api)

if(OS_WINDOWS)
    set(SoundTouch_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/BPMDetect.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSampleBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSamplePipe.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/SoundTouch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/STTypes.h)
elseif(OS_MACOS)
    set(SoundTouch_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/BPMDetect.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSampleBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/FIFOSamplePipe.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/SoundTouch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/STTypes.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SoundTouch/include/soundtouch_config.h)
endif()
source_group("SoundTouchLib" FILES ${SoundTouch_HEADERS})

target_link_libraries(prism-sound-touch debug ${ST_LIBRARIES_DEBUG})
target_link_libraries(prism-sound-touch optimized ${ST_LIBRARIES_RELEASE})
set_target_properties(prism-sound-touch PROPERTIES FOLDER prism/plugins/source)

copy_prism_plugin_binary(prism-sound-touch)

