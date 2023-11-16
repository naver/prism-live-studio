

function(setup_bundle_info target)
  set(MACOSX_BUNDLE_EXECUTABLE_NAME
    "${target}"
    PARENT_SCOPE)
  set(MACOSX_BUNDLE_GUI_IDENTIFIER
    "com.prismproject.${target}"
    PARENT_SCOPE)
  set(MACOS_BUNDLE_PRISM_VERSION ${PRISM_VERSION_MAJOR}.${PRISM_VERSION_MINOR}.${PRISM_VERSION_PATCH})
  set(MACOSX_BUNDLE_BUNDLE_VERSION
    ${MACOS_BUNDLE_PRISM_VERSION}
    PARENT_SCOPE)
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING
    ${MACOS_BUNDLE_PRISM_VERSION}
    PARENT_SCOPE)

  set_target_properties(
    ${target}
    PROPERTIES OUTPUT_NAME ${target}
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST $ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/Info.plist.in
  )

  set_target_properties(
    ${target}
    PROPERTIES BUILD_WITH_INSTALL_RPATH OFF
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.prismproject.${target}"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/entitlements.plist")
endfunction()

function(setup_logger_app target)
  setup_bundle_info(${target})
  set_property(GLOBAL APPEND PROPERTY PRISM_MACHO_LIST "${target}")
  copy_prism_bundle_macho_bundle(${target})
endfunction()

# Helper function to set up launcher app target
function(setup_launcher_app target)
  setup_bundle_info(${target})
  set_property(GLOBAL APPEND PROPERTY PRISM_MACHO_LIST "${target}")
  copy_prism_bundle_macho_bundle(${target})
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
              "-DSOURCE_BUNDLE_CONTENTS_PATH=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
              "-DSOURCE_HELPER_INFO_PLIST_DIR=$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS"
              "-DDEST_HELPER_INFO_PLST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
              "-DSOURCE_HELPER_NAME=PRISMLiveStudio"
              "-DDEST_HELPER_NAME=${target}"
              -P "$ENV{PRISM_SRC_DIR}/cmake/macOS/prism_runtime_copy_data.cmake"
    COMMENT "Installing obs app framework for development"
    VERBATIM)
endfunction()

function(copy_prism_bundle_macho_bundle target)
  set(PRISM_BUNDLE_CONTENT_PATH ${PRISMLiveStudio_BINARY_DIR}/${CMAKE_BUILD_TYPE}/PRISMLiveStudio.app/Contents)
  message(STATUS "PRISM_BUNDLE_CONTENT_PATH is ${PRISM_BUNDLE_CONTENT_PATH}")
  file(GLOB _MACOS_LISTS
     "${PRISM_BUNDLE_CONTENT_PATH}/MacOS/*")
  message(STATUS "_MACOS_LISTS is ${_MACOS_LISTS}")
  install(
    PROGRAMS ${_MACOS_LISTS}
    DESTINATION "MacOS"
    COMPONENT install_macho_data_dev
    EXCLUDE_FROM_ALL)
  install(
    DIRECTORY ${PRISM_BUNDLE_CONTENT_PATH}/Frameworks/
    DESTINATION "Frameworks"
    COMPONENT install_macho_data_dev
    USE_SOURCE_PERMISSIONS
    EXCLUDE_FROM_ALL)
  install(
    DIRECTORY ${PRISM_BUNDLE_CONTENT_PATH}/PlugIns/
    DESTINATION "PlugIns"
    COMPONENT install_macho_data_dev
    EXCLUDE_FROM_ALL)
  install(
    DIRECTORY ${PRISM_BUNDLE_CONTENT_PATH}/Resources/
    DESTINATION "Resources"
    USE_SOURCE_PERMISSIONS
    COMPONENT install_macho_data_dev
    EXCLUDE_FROM_ALL)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG> --prefix $<TARGET_BUNDLE_CONTENT_DIR:${target}> --component install_macho_data_dev
    VERBATIM)
endfunction()