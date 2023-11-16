project(prism-spectralizer)

message(STATUS "PRISM Plugin: prism-spectralizer")

add_definitions(-D_PRISM_PLUGIN)

set(CMAKE_AUTORCC True)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
set(FFTW_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/fftw3/include)

if(OS_WINDOWS)
set(FFTW_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/fftw3/lib/64bit/fftw3.lib)
elseif(OS_MACOS)
set(FFTW_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/fftw3/lib/mac/libfftw3.dylib)
endif()

include_directories(${FFTW_INCLUDE_DIRS})

file(GLOB_RECURSE src_files *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE hdr_files *.h *.hpp *.hxx *.hh)

add_library(prism-spectralizer MODULE ${src_files} ${hdr_files})
add_library(prism::prism-spectralizer ALIAS prism-spectralizer)
target_link_libraries(prism-spectralizer  OBS::libobs OBS::frontend-api common-libs::utils-api  common-libs::log prism::frontend-api ${FFTW_LIBRARIES})
target_sources(prism-spectralizer PRIVATE prism-spectralizer.qrc)
set_target_properties(prism-spectralizer PROPERTIES FOLDER prism/plugins/source)

copy_prism_plugin_binary(prism-spectralizer)
if(OS_WINDOWS)
copy_prism_plugin_dep(prism-spectralizer fftw3 ${CMAKE_CURRENT_SOURCE_DIR}/fftw3/lib/64bit prism-plugins/64bit)
elseif(OS_MACOS)
copy_dylib_path(${FFTW_LIBRARIES})
endif()


