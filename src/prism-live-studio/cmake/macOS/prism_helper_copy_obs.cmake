message(STATUS "Copy OBS data.")

set(OBS_BUILD_DIR $ENV{OBS_BUILD_DIR})
set(BUILD_CONFIG ${CMAKE_BUILD_TYPE})
set(OBS_UI_APP_CONTENT $ENV{OBS_BUILD_DIR}/UI/${BUILD_CONFIG}/OBS.app/Contents)
set(OBS_DEPENDENCY_PREFIX ${CMAKE_PREFIX_PATH})
set(OBS_VCAM_EXTENSION_DIR $ENV{OBS_BUILD_DIR}/plugins/mac-virtualcam/${BUILD_CONFIG}/)

message(STATUS "BUILD_CONFIG=${BUILD_CONFIG}")
message(STATUS "OBS_BUILD_DIR is ${OBS_BUILD_DIR}")
message(STATUS "OBS_UI_APP_CONTENT=${OBS_UI_APP_CONTENT}")
message(STATUS "OBS_DEPENDENCY_PREFIX is ${OBS_DEPENDENCY_PREFIX}")
message(STATUS "OBS_VCAM_EXTENSION_DIR=${OBS_VCAM_EXTENSION_DIR}")

function(copy_obs_dependencies_dylib)
  foreach(_PREFIX_PATH IN LISTS OBS_DEPENDENCY_PREFIX)
    file(GLOB _DYLIBS "${_PREFIX_PATH}/lib/*.dylib")
    install(
      FILES ${_DYLIBS}
      DESTINATION "Frameworks"
      COMPONENT obs_resources_dev
      EXCLUDE_FROM_ALL)
  endforeach()
endfunction()

function(copy_obs_dylib)
  get_property(OBS_DYLIB_LIST GLOBAL PROPERTY OBS_DYLIB_LIST)
  message(STATUS "OBS_DYLIB_LIST is ${OBS_DYLIB_LIST}")
  set(OBS_DYLIB_LIST
    ${OBS_BUILD_DIR}/deps/obs-scripting/${BUILD_CONFIG}/
    ${OBS_BUILD_DIR}/deps/glad/${BUILD_CONFIG}/
    ${OBS_BUILD_DIR}/UI/obs-frontend-api/${BUILD_CONFIG}/
    ${OBS_BUILD_DIR}/libobs/${BUILD_CONFIG}/
    ${OBS_BUILD_DIR}/libobs-opengl/${BUILD_CONFIG}/
  )
  foreach(_OBS_DYLIB IN LISTS OBS_DYLIB_LIST)
  message(STATUS "_OBS_DYLIB is ${_OBS_DYLIB}")
    install(
      DIRECTORY ${_OBS_DYLIB}
      DESTINATION "Frameworks"
      USE_SOURCE_PERMISSIONS
      COMPONENT obs_resources_dev
      EXCLUDE_FROM_ALL)
  endforeach()
endfunction()


function(copy_obs_app_plugin)
  set(OBS_APP_PLUGIN_FOLDER ${OBS_UI_APP_CONTENT}/PlugIns/)
  install(
    DIRECTORY ${OBS_APP_PLUGIN_FOLDER} 
    DESTINATION "PlugIns" 
    USE_SOURCE_PERMISSIONS
    COMPONENT obs_resources_dev 
    EXCLUDE_FROM_ALL)
endfunction()

function(setup_obs_app_framework target)

  set(OBS_APP_FRAMEWORK_FOLDER ${OBS_UI_APP_CONTENT}/Frameworks/)
  install(
    DIRECTORY ${OBS_APP_FRAMEWORK_FOLDER}
    DESTINATION "Frameworks"
    USE_SOURCE_PERMISSIONS
    COMPONENT obs_resources_dev
    EXCLUDE_FROM_ALL)
endfunction()

function(setup_obs_ffmpeg_mux)
  set(OBS_FFMPEG_MUX ${OBS_UI_APP_CONTENT}/MacOS/obs-ffmpeg-mux)
  #execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${OBS_FFMPEG_MUX} ${DEST_FOLDER_PATH}/MacOS)
  install(
      PROGRAMS ${OBS_FFMPEG_MUX}
      DESTINATION "MacOS"
      COMPONENT obs_resources_dev
      EXCLUDE_FROM_ALL)
endfunction()

function(setup_obs_python)
  set(OBS_PYTHON_PATH ${OBS_UI_APP_CONTENT}/Resources/obspython.py)
  #execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${OBS_PYTHON_PATH} ${DEST_FOLDER_PATH}/Resources)
  install(
    FILES ${OBS_PYTHON_PATH}
    DESTINATION "Resources"
    COMPONENT obs_resources_dev
    EXCLUDE_FROM_ALL)
endfunction()

function(setup_obs_mac_virtualcam)
  install(
      DIRECTORY ${OBS_VCAM_EXTENSION_DIR}/com.prismlive.prismlivestudio.mac-camera-extension.systemextension
      DESTINATION "Library/SystemExtensions"
      USE_SOURCE_PERMISSIONS
      COMPONENT obs_resources_dev
      EXCLUDE_FROM_ALL)
endfunction()

function(install_obs_resources target)
  copy_obs_app_plugin()
  setup_obs_app_framework(${target})
  setup_obs_ffmpeg_mux()
  setup_obs_python()
  setup_obs_mac_virtualcam()
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG> --prefix $<TARGET_BUNDLE_CONTENT_DIR:${target}> --component obs_resources_dev
    # COMMAND plutil -replace CFBundleShortVersionString -string "${PRISM_VERSION_SHORT}" $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/prism-mac-virtualcam.plugin/Contents/Info.plist
    # COMMAND plutil -replace CFBundleVersion -string "${PRISM_VERSION_BUILD}" $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/prism-mac-virtualcam.plugin/Contents/Info.plist
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$ENV{PRISM_SRC_DIR}/PRISMLiveStudio/prism-ui/resource/LUTs" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/PlugIns/obs-filters.plugin/Contents/Resources/LUTs"
    #COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "/Users/zhongling/Desktop/com.prismlive.prismlivestudio.mac-camera-extension.systemextension" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Library/SystemExtensions"
    VERBATIM)
endfunction()

