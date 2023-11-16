include(launcher_helper)
include(prism_helper_copy_obs)

function(copy_excutable_target target)
  string(TOLOWER ${target} lowercaseTarget)

    set_target_properties( ${target} PROPERTIES 
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER  "com.prismlive.${lowercaseTarget}"
      OUTPUT_NAME ${target}
    )

  set_property(GLOBAL APPEND PROPERTY EXCUTABLE_TARGET_LIST "${target}")
endfunction()

function(copy_framework_target target)
    set_property(GLOBAL APPEND PROPERTY FRAMEWORK_TARGET_LIST "${target}")
    set_property(GLOBAL APPEND PROPERTY PRISM_FRAMEWORK_LIST "${target}")
endfunction()

function(copy_static_lib_target target)
    set_property(GLOBAL APPEND PROPERTY STATIC_LIB_TARGET_LIST "${target}")
    set_property(GLOBAL APPEND PROPERTY PRISM_FRAMEWORK_LIST "${target}")
endfunction()

function(copy_dylib_target target)
    set_property(GLOBAL APPEND PROPERTY DYLIB_TARGET_LIST "${target}")
    set_property(GLOBAL APPEND PROPERTY PRISM_FRAMEWORK_LIST "${target}")
endfunction()

#direct copy dylib path to prism frameworks path when post build
function(copy_dylib_path dylibPath)
    set_property(GLOBAL APPEND PROPERTY DYLIB_ORIGIANL_PATH_LIST "${dylibPath}")
endfunction()

# Helper function to set up OBS plugin targets
function(setup_plugin_target target)
 set(MACOSX_PLUGIN_EXECUTABLE_NAME "${target}")
  set(MACOSX_PLUGIN_GUI_IDENTIFIER "$ENV{PRISM_PRODUCT_IDENTIFIER_PRESUFF}.${target}")
  set(MACOSX_PLUGIN_BUNDLE_NAME "${target}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "${OBS_VERSION_CANONICAL}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${OBS_BUILD_NUMBER}")
  set(MACOSX_PLUGIN_BUNDLE_TYPE "BNDL")

   configure_file("$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/Plugin-Info.plist.in"  "${CMAKE_CURRENT_BINARY_DIR}/info.plist")

   set_target_properties(
    ${target}
    PROPERTIES OUTPUT_NAME ${target}
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/info.plist
  )


  set_target_properties(
    ${target}
    PROPERTIES BUNDLE ON
               BUNDLE_EXTENSION "plugin"
               OUTPUT_NAME "${target}"
               MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/info.plist"
               XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.prismlive.${target}"
               XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/entitlements.plist")

  set_property(GLOBAL APPEND PROPERTY PRISM_MODULE_LIST "${target}")
  copy_folder_to_target_folder(${CMAKE_CURRENT_SOURCE_DIR}/data ${target} Resources)
endfunction()

function(setup_plugin_extra_dependency target)
   set_property(GLOBAL APPEND PROPERTY PLUGIN_EXTRA_DEPENDENCY "${target}")
endfunction()

function(install_plugin_extra_dependency target)
    get_property(PLUGIN_EXTRA_DEPENDENCY GLOBAL PROPERTY PLUGIN_EXTRA_DEPENDENCY)
  if(NOT DEFINED PLUGIN_EXTRA_DEPENDENCY)
    message("not defined environment PLUGIN_EXTRA_DEPENDENCY")  
    return()
  endif()
    target_link_libraries(${target} PRIVATE ${PLUGIN_EXTRA_DEPENDENCY})

endfunction()

function(install_excutable_target_list target)
  get_property(EXCUTABLE_TARGET_LIST GLOBAL PROPERTY EXCUTABLE_TARGET_LIST)

  if(NOT DEFINED EXCUTABLE_TARGET_LIST)
    message("not defined environment EXCUTABLE_TARGET_LIST")  
    return()
  endif()

  add_dependencies(${target} ${EXCUTABLE_TARGET_LIST})

  foreach(_EXCUTABLE_TARGET IN LISTS EXCUTABLE_TARGET_LIST)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${_EXCUTABLE_TARGET}> $<TARGET_BUNDLE_CONTENT_DIR:${target}>/MacOS
      COMMENT "Installing PRISM Executable List"
      VERBATIM)
  endforeach()
  
endfunction()

function(install_framework_target_list target)
  get_property(FRAMEWORK_TARGET_LIST GLOBAL PROPERTY FRAMEWORK_TARGET_LIST)
  list(LENGTH FRAMEWORK_TARGET_LIST _LEN)

  if(_LEN GREATER 0)
    add_dependencies(${target} ${FRAMEWORK_TARGET_LIST})

    foreach(_FRAMEWORK_TARGET IN LISTS FRAMEWORK_TARGET_LIST)
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${_FRAMEWORK_TARGET}> $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks
        COMMENT "Installing PRISM Framwork List"
        VERBATIM)
    endforeach()
  endif()
endfunction()

function(install_dylib_target_list target)
  get_property(DYLIB_TARGET_LIST GLOBAL PROPERTY DYLIB_TARGET_LIST)
  add_dependencies(${target} ${DYLIB_TARGET_LIST})
  install(
    TARGETS ${DYLIB_TARGET_LIST}
    DESTINATION "Frameworks"
    COMPONENT prism_dylib_dev
    EXCLUDE_FROM_ALL)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} --install .. --config $<CONFIG> --prefix $<TARGET_BUNDLE_CONTENT_DIR:${target}> --component prism_dylib_dev
    VERBATIM)

endfunction()

function(install_dylib_path_list target)
  get_property(DYLIB_ORIGIANL_PATH_LIST GLOBAL PROPERTY DYLIB_ORIGIANL_PATH_LIST)
  list(LENGTH DYLIB_ORIGIANL_PATH_LIST _LEN)

  if(_LEN GREATER 0)

    foreach(_DYLIB_ORIGIANL_PATH IN LISTS DYLIB_ORIGIANL_PATH_LIST)
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DLIBRARY=${_DYLIB_ORIGIANL_PATH}" 
                "-DDESTINATION=$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks" 
                -P "$ENV{PRISM_SRC_DIR}/cmake/macOS/prism_copy_symlink_chain.cmake"
        COMMENT "Installing PRISM Original Framwork List"
        VERBATIM)
      endforeach()
    endif()
endfunction()

# Helper function to set-up OBS plugins and helper binaries for macOS bundling
function(install_prism_plugin_list target)
  get_property(PRISM_MODULE_LIST GLOBAL PROPERTY PRISM_MODULE_LIST)
  list(LENGTH PRISM_MODULE_LIST _LEN)

  if(_LEN GREATER 0)
    add_dependencies(${target} ${PRISM_MODULE_LIST})

    install(
      TARGETS ${PRISM_MODULE_LIST}
      LIBRARY
      DESTINATION "PlugIns"
      COMPONENT prism_plugins_dev
      EXCLUDE_FROM_ALL)

    install(
      TARGETS ${PRISM_MODULE_LIST}
      LIBRARY
      DESTINATION $<TARGET_FILE_BASE_NAME:${target}>.app/Contents/PlugIns
      COMPONENT prism_plugins
      NAMELINK_COMPONENT ${target}_Development)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG> --prefix $<TARGET_BUNDLE_CONTENT_DIR:${target}> --component prism_plugins_dev
      VERBATIM)
  endif()
endfunction()

# Helper function to set-up OBS frameworks for macOS bundling
function(setup_prism_frameworks target)
  get_property(PRISM_FRAMEWORK_LIST GLOBAL PROPERTY PRISM_FRAMEWORK_LIST)
  install(
    TARGETS ${PRISM_FRAMEWORK_LIST}
    RUNTIME
      DESTINATION "$<TARGET_FILE_BASE_NAME:${target}>.app/Contents/Frameworks/"
      COMPONENT prism_frameworks
    LIBRARY
      DESTINATION "$<TARGET_FILE_BASE_NAME:${target}>.app/Contents/Frameworks/"
      COMPONENT prism_frameworks
    FRAMEWORK
      DESTINATION "$<TARGET_FILE_BASE_NAME:${target}>.app/Contents/Frameworks/"
      COMPONENT prism_frameworks
    PUBLIC_HEADER
      DESTINATION "${OBS_INCLUDE_DESTINATION}"
      COMPONENT prism_libraries
      EXCLUDE_FROM_ALL)
endfunction()

function(install_common_data target)
  get_property(COMMON_DATA_LIST GLOBAL PROPERTY COMMON_DATA_LIST)

  foreach(COMMON_DATA_PAIR IN LISTS COMMON_DATA_LIST)
    string(FIND "${COMMON_DATA_PAIR}" ":" pos)
    string(SUBSTRING "${COMMON_DATA_PAIR}" 0 "${pos}" data_src_path)
    math(EXPR pos "${pos} + 1")
    string(SUBSTRING "${COMMON_DATA_PAIR}" "${pos}" -1 data_output_path)

    if(IS_DIRECTORY "${data_src_path}")
      install(
        DIRECTORY ${data_src_path}
        DESTINATION "${data_output_path}"
        COMPONENT install_common_data_dev
        EXCLUDE_FROM_ALL)
    else()
      install(
        FILES ${data_src_path}
        DESTINATION "${data_output_path}"
        COMPONENT install_common_data_dev
        EXCLUDE_FROM_ALL)
    endif()
  endforeach()

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG> --prefix $<TARGET_BUNDLE_CONTENT_DIR:${target}> --component install_common_data_dev
    VERBATIM)
endfunction()

function(copy_mac_common_libs_data data_src_path data_output_path)
  set(source_data_path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path})

  if(IS_DIRECTORY "${source_data_path}")
    set(path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path}/:Resources/${data_output_path})
  else()
    set(path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path}:Resources/${data_output_path})
  endif()

  set_property(GLOBAL APPEND PROPERTY COMMON_DATA_LIST ${path})
endfunction()

function(copy_mac_framework_data data_src_path)
  set(path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path}:Frameworks)
  set_property(GLOBAL APPEND PROPERTY COMMON_DATA_LIST ${path})
endfunction()

function(copy_mac_dylibs_data data_src_path)
  set(path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path}/:Frameworks)
  set_property(GLOBAL APPEND PROPERTY COMMON_DATA_LIST ${path})
endfunction()

function(copy_mac_dylib_data)
  set(path ${CMAKE_CURRENT_SOURCE_DIR}/${data_src_path}:Frameworks)
  set_property(GLOBAL APPEND PROPERTY COMMON_DATA_LIST ${path})
endfunction()


function(copy_file_to_target_folder filePath target destDir)  
  target_sources(${target} PRIVATE ${filePath})
  set_source_files_properties(
    ${filePath} PROPERTIES MACOSX_PACKAGE_LOCATION "${destDir}")
endfunction()

function(copy_file_to_target_folder_with_group filePath sourceFolder target destFolder)
  file(RELATIVE_PATH _RELATIVE_PATH ${sourceFolder} ${filePath})
  get_filename_component(_RELATIVE_PATH "${_RELATIVE_PATH}" PATH)
  target_sources(${target} PRIVATE ${filePath})
  set_source_files_properties(${filePath} PROPERTIES MACOSX_PACKAGE_LOCATION "${destFolder}/${_RELATIVE_PATH}")

  string(REPLACE "\\" "\\\\" _GROUP_NAME "${_RELATIVE_PATH}")
  source_group("Resources\\${_GROUP_NAME}" FILES ${filePath})
endfunction()

function(copy_folder_to_target_folder sourceFolder target destFolder)
  if(EXISTS "${sourceFolder}")
    file(GLOB_RECURSE _SOURCE_FILES "${sourceFolder}/*")

    foreach(_SOURCE_FILE IN LISTS _SOURCE_FILES)
      copy_file_to_target_folder_with_group(${_SOURCE_FILE} ${sourceFolder} ${target} ${destFolder})
    endforeach()
  endif()
endfunction()

function(install_file_resources filePath target)
  file(RELATIVE_PATH _RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/data ${filePath})
  get_filename_component(_RELATIVE_PATH "${_RELATIVE_PATH}" PATH)
  target_sources(${target} PRIVATE ${filePath})
  set_source_files_properties(
    ${filePath} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${_RELATIVE_PATH}")
  string(REPLACE "\\" "\\\\" _GROUP_NAME "${_RELATIVE_PATH}")
  source_group("Resources\\${_GROUP_NAME}" FILES ${filePath})
endfunction()

function(set_extra_dylib_path dylibPath)
  set_property(GLOBAL APPEND PROPERTY EXTRA_DYLIB_PATH "${dylibPath}")
endfunction()

#Helper function to set up plugin resources inside plugin bundle
function(install_bundle_resources target)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(GLOB_RECURSE _DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
    foreach(_DATA_FILE IN LISTS _DATA_FILES)
      install_file_resources(${_DATA_FILE} ${target})
    endforeach()
  endif()
endfunction()

function(install_and_codesign)
  get_property(EXTRA_DYLIB_PATH GLOBAL PROPERTY EXTRA_DYLIB_PATH)
  message("EXTRA_DYLIB_PATH: ${EXTRA_DYLIB_PATH}")

  install(
    CODE "
    set(_BUNDLENAME \"$<TARGET_FILE_BASE_NAME:${target}>.app\")
    set(_BUNDLER_COMMAND \"$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/dylibbundler\")
    set(_CODESIGN_IDENTITY \"${PRISM_BUNDLE_CODESIGN_IDENTITY}\")
    set(_OBS_BUNDLE_PATH \"$ENV{OBS_BUILD_DIR}/install/OBS.app\")
    set(_CODESIGN_ENTITLEMENTS \"$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS\")
    set(_VERBOSE_FLAG \"$ENV{VERBOSE}\")
    set(_QUIET_FLAG \"$ENV{QUIET}\")
    set(_EXTRA_DEPS \"${EXTRA_DYLIB_PATH}\")
    set(_LUT_PATH \"$ENV{PRISM_SRC_DIR}/PRISMLiveStudio/prism-ui/resource/LUTs\")"
    COMPONENT prism_bundles)
  
endfunction()

# Helper function to set up OBS app target
function(setup_prism_app target)
  set_target_properties(
    ${target}
    PROPERTIES BUILD_WITH_INSTALL_RPATH OFF
               XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
               "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/entitlements.plist"
               XCODE_SCHEME_ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

  install(TARGETS ${target} BUNDLE DESTINATION "." COMPONENT prism_app)
  message(STATUS "_BUNDLER_COMMAND is ${_BUNDLER_COMMAND}")
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE STRING "Directory to install OBS to after building" FORCE)
  
  #install_bundle_resources(${target})
  install_obs_resources(${target})
  install_common_data(${target})
  install_excutable_target_list(${target}) 
  install_prism_plugin_list(${target})
  setup_prism_frameworks(${target})
  install_framework_target_list(${target})
  install_dylib_path_list(${target})
  install_plugin_extra_dependency(${target})

  install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --install $ENV{OBS_BUILD_DIR} --config ${CMAKE_BUILD_TYPE})" COMPONENT prism_install_obs)
  install_and_codesign()

add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_BUNDLE_CONTENTS_PATH=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
            "-DDEST_BUNDLE_CONTENTS_PATH=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
            "-DSOURCE_HELPER_INFO_PLIST_DIR=$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS"
            "-DDEST_HELPER_INFO_PLST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DSOURCE_HELPER_NAME=OBS"
            "-DDEST_HELPER_NAME=${target}"
            -P "$ENV{PRISM_SRC_DIR}/cmake/macOS/prism_runtime_copy_data.cmake"
    COMMENT "Installing obs app framework for development"
    VERBATIM)

add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_BUNDLE_CONTENTS_PATH=$ENV{OBS_BUILD_DIR}/UI/${BUILD_CONFIG}/OBS.app/Contents"
            "-DDEST_BUNDLE_CONTENTS_PATH=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
            "-DSOURCE_HELPER_INFO_PLIST_DIR=$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS"
            "-DDEST_HELPER_INFO_PLST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DSOURCE_HELPER_NAME=OBS"
            "-DDEST_HELPER_NAME=PRISMLauncher"
            -P "$ENV{PRISM_SRC_DIR}/cmake/macOS/prism_runtime_copy_data.cmake"
    COMMENT "Installing obs app framework for development"
    VERBATIM)

endfunction()
