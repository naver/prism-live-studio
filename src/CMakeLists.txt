cmake_minimum_required(VERSION 3.16)



list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "$ENV{OBS_SRC_DIR}/cmake/Modules")

project(prism-live-studio VERSION $ENV{VERSION})

set(PRISM_VERSION ${CMAKE_PROJECT_VERSION})
set(PRISM_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(PRISM_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(PRISM_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
set(PRISM_VERSION_TWEAK ${CMAKE_PROJECT_VERSION_TWEAK})
set(PRISM_COMMENTS $ENV{PRISM_COMMENTS})
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(OS_WINDOWS OFF)
  set(OS_MACOS ON)
  set(OS_POSIX ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(OS_WINDOWS ON)
  set(OS_MACOS OFF)
  set(OS_POSIX OFF)
endif()


if(OS_MACOS)

  list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake/macOS")
  list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake/Modules")
  include(prism_helper)
  include(PrismDefaults_macOS)
  
  setup_prism_project()

endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(COMMON_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common-libs")
set(EXE_RC_PATH "$ENV{PRISM_SRC_DIR}/cmake/bundle/windows/prism-exe.rc.in")
set(LIB_RC_PATH "$ENV{PRISM_SRC_DIR}/cmake/bundle/windows/prism-module.rc.in")

function(prism_module target description)
    if(OS_WINDOWS)
      set(MODULE_DESCRIPTION ${description})
      configure_file($ENV{PRISM_SRC_DIR}/cmake/bundle/windows/prism-module.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
      target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
    elseif(OS_MACOS)
      set_target_properties(${target} PROPERTIES SOVERSION "1")
    endif()
endfunction()

function(prism_exe target description)
    if(OS_WINDOWS)
      set(MODULE_DESCRIPTION ${description})
      configure_file($ENV{PRISM_SRC_DIR}/cmake/bundle/windows/prism-exe.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
      target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)
    elseif(OS_MACOS)
      
    endif()
endfunction()

function(copy_target target to_copy binary_src_path binary_output_path)
  add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DCONFIG=$<CONFIGURATION>"
      "-DSRC_PATH=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DDEV_OUTPUT_PATH=$ENV{DEV_OUTPUT_DIR}"
      "-DBINARY_SRC_PATH=${binary_src_path}"
      "-DBINARY_OUTPUT_PATH=${binary_output_path}"
      "-DARCH=$ENV{ARCH}"
      "-DTARGET=${to_copy}"
      "-DOS_WINDOWS=${OS_WINDOWS}"
      "-DOS_MACOS=${OS_MACOS}"
      -P "$ENV{PRISM_SRC_DIR}/cmake/copy_target.cmake"
    VERBATIM)
endfunction()

function(copy_data target to_copy data_src_path data_output_path)
  add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DCONFIG=$<CONFIGURATION>"
      "-DSRC_PATH=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DDEV_OUTPUT_PATH=$ENV{DEV_OUTPUT_DIR}"
      "-DDATA_SRC_PATH=${data_src_path}"
      "-DDATA_OUTPUT_PATH=${data_output_path}"
      "-DARCH=$ENV{ARCH}"
      "-DTARGET=${to_copy}"
      "-DOS_WINDOWS=${OS_WINDOWS}"
      "-DOS_MACOS=${OS_MACOS}"
      -P "$ENV{PRISM_SRC_DIR}/cmake/copy_data.cmake"
    VERBATIM)
endfunction()

function(deploy_qt target)
  add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DCONFIG=$<CONFIGURATION>"
      "-DQT_DIR=$ENV{QTDIR}"
      "-DDEV_OUTPUT_DIR=$ENV{DEV_OUTPUT_DIR}"
      "-DTARGET=${target}"
      "-DOS_WINDOWS=${OS_WINDOWS}"
      "-DOS_MACOS=${OS_MACOS}"
      -P "$ENV{PRISM_SRC_DIR}/cmake/deploy_qt.cmake"
    VERBATIM)
endfunction()

function(common_libs_module target description)
  prism_module(${target} "PRISM ${description}")
endfunction()

function(common_libs_exe target description)
  prism_exe(${target} "PRISM ${description}")
endfunction()

function(copy_common_libs_binary target)
  if(OS_WINDOWS)
    copy_target(${target} ${target} "" bin/64bit)
  elseif(OS_MACOS)
    copy_dylib_target(${target})
  endif()
endfunction()

function(copy_common_libs_data target data_src_path data_output_path)
  if(OS_WINDOWS)
    copy_data(${target} ${target} ${data_src_path} ${data_output_path})
  elseif(OS_MACOS)
    copy_mac_common_libs_data(${data_src_path} ${data_output_path})
  endif()
endfunction()

function(copy_prism_binary target)
  if(OS_WINDOWS)
    copy_target(${target} ${target} "" bin/64bit)
  elseif(OS_MACOS)
    copy_excutable_target(${target})
  endif()
endfunction()

function(copy_prism_data target)
  if(OS_WINDOWS)
    copy_data(${target} ${target} data data/prism-studio)
  endif()
endfunction()

function(copy_setup_target target translations)
  if(OS_WINDOWS)
    copy_target(${target} ${target} "" bin/64bit)
    copy_data(${target} ${target} "${translations}" bin/64bit/translations)
  elseif(OS_MACOS)
  endif()
endfunction()

function(copy_prism_common_binary target)
  add_dependencies(${target} obs-build)
  if(OS_WINDOWS)
    copy_target(${target} ${target} "" bin/64bit)
  elseif(OS_MACOS)
    copy_dylib_target(${target})
  endif()
endfunction()

function(copy_prism_plugin_binary target)
  add_dependencies(${target} obs-build)
  if(OS_WINDOWS)
    copy_target(${target} ${target} "" prism-plugins/64bit)
    copy_data(${target} ${target} data data/prism-plugins/${target})
  elseif(OS_MACOS)
    setup_plugin_target(${target})
  endif()
endfunction()

function(copy_prism_plugin_dep target name src_dir dest_dir)
  if (OS_WINDOWS)
    copy_target(${target} ${name} "${src_dir}" ${dest_dir})
  elseif(OS_MACOS)
  endif()
endfunction()

option(ENABLE_HEVC "Enable HEVC encoders" ON)
if(ENABLE_HEVC)
  add_compile_definitions(ENABLE_HEVC)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(PRISM_BUILD_TYPE_DEBUG)
endif()

if(OS_WINDOWS)
  add_compile_options(/MP)
endif()

add_subdirectory(common-libs)
add_subdirectory(prism-live-studio)
