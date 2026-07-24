if(POLICY CMP0007)
  cmake_policy(SET CMP0007 NEW)
endif()
if(POLICY CMP0009)
  cmake_policy(SET CMP0009 NEW)
endif()
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif()

message(STATUS "bundleutils: PRISM: copy obs app bundle to prism app bundle")

set(QTDIR ${_PRISM_QT_DIR})
message(STATUS "bundleutils: PRISM: bundleutils QTDIR: ${QTDIR}/lib")
if(NOT DEFINED QTDIR)
    message(FATAL_ERROR "not defined environment variable:QTDIR")  
endif()

set(OBS_BUNDLE_CONTENTS_PATH "${_OBS_BUNDLE_PATH}/Contents")
set(PRISM_BUNDLE_CONTENTS_PATH "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents")

# Find all dylibs, frameworks and other code elements inside bundle
# file(GLOB _DYLIBS "${OBS_BUNDLE_CONTENTS_PATH}/Frameworks/*")
# file(GLOB _PlugIns "${OBS_BUNDLE_CONTENTS_PATH}/PlugIns/*")
file(GLOB _qtls "${QTDIR}/plugins/tls/*")
file(GLOB _qtmultimedia "${QTDIR}/plugins/multimedia/*")
file(GLOB _LUTs "${_LUT_PATH}/*")

set(_OBS_PLUGINS_REMOVE "")
file(GLOB _sources_list LIST_DIRECTORIES true "${_OBS_BUNDLE_PATH}/../../UI/frontend-plugins/*")
foreach(dir ${_sources_list})
    IF(IS_DIRECTORY ${dir})
      get_filename_component(dir_name "${dir}" NAME_WLE)
      list(APPEND _OBS_PLUGINS_REMOVE "${OBS_BUNDLE_CONTENTS_PATH}/PlugIns/${dir_name}.plugin")
    ELSE()
        CONTINUE()
    ENDIF()
endforeach()
unset(_sources_list)
list(REMOVE_ITEM _PlugIns ${_OBS_PLUGINS_REMOVE})
message("bundleutils: prism ignore obs plugins: ${_OBS_PLUGINS_REMOVE}")
unset(_OBS_PLUGINS_REMOVE)


file(COPY ${_DYLIBS}  DESTINATION ${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks)
file(COPY ${_PlugIns} DESTINATION ${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns)
file(COPY ${_qtls} DESTINATION ${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/tls)
file(COPY ${_qtmultimedia} DESTINATION ${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/multimedia)
file(COPY ${_LUTs} DESTINATION "${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/obs-filters.plugin/Contents/Resources/LUTs") #copy LUTs

# file(GLOB _OBS_EXCUTABLES "${OBS_BUNDLE_CONTENTS_PATH}/MacOS/*")
# foreach(_obs_exe ${_OBS_EXCUTABLES})
#   get_filename_component(_obs_name "${_obs_exe}" NAME_WLE)
#   if(NOT ${_obs_name} STREQUAL "OBS")
#     file(COPY ${_obs_exe} DESTINATION ${PRISM_BUNDLE_CONTENTS_PATH}/MacOS)
#   endif()
#   unset(_obs_exe)
# endforeach()
# unset(_OBS_EXCUTABLES)
message("bundleutils: prism: copy obs app bundle to prism app bundle complected")

# Add additional search paths for dylibbundler
list(APPEND _FIXUP_BUNDLES "-s \"${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks\"")
list(APPEND _FIXUP_BUNDLES "-s \"${QTDIR}/lib\"")

string(REPLACE ";" "\" -s \"" _EXTRA_DEPS "${_EXTRA_DEPS}")
list(APPEND _FIXUP_BUNDLES "-s \"${_EXTRA_DEPS}\"")

foreach(_PREFIX_PATH IN LISTS _DEPENDENCY_PREFIX)
  list(APPEND _FIXUP_BUNDLES " -s \"${_PREFIX_PATH}/lib\"")
  file(GLOB _DYLIBS "${_PREFIX_PATH}/lib/*.dylib")
  file(
    COPY ${_DYLIBS}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/Frameworks
    FOLLOW_SYMLINK_CHAIN)
  unset(_DYLIBS)
endforeach()


# Find all modules (plugin and standalone)
file(GLOB _OBS_PLUGINS "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/*.plugin")
file(GLOB _OBS_SCRIPTING_PLUGINS "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}/Contents/PlugIns/*.so")

# Add modules to fixups
foreach(_OBS_PLUGIN IN LISTS _OBS_PLUGINS)
  get_filename_component(PLUGIN_NAME "${_OBS_PLUGIN}" NAME_WLE)
  list(APPEND _FIXUP_BUNDLES " -x \"${_OBS_PLUGIN}/Contents/MacOS/${PLUGIN_NAME}\"")
endforeach()

file(GLOB _PRISM_EXCUTABLES "${PRISM_BUNDLE_CONTENTS_PATH}/MacOS/*")
foreach(_prism_exe ${_PRISM_EXCUTABLES})
  get_filename_component(_prism_name "${_prism_exe}" NAME_WLE)
  if(NOT ${_prism_name} STREQUAL "PRISMLiveStudio")
    list(APPEND _FIXUP_BUNDLES " -x \"${_prism_exe}\"")
  endif()
  unset(_prism_name)
endforeach()
unset(_PRISM_EXCUTABLES)

list(REMOVE_DUPLICATES _FIXUP_BUNDLES)
string(REPLACE ";" " " _FIXUP_BUNDLES "${_FIXUP_BUNDLES}")

message("bundleutils: PRISM: Bundle linked libraries and dependencies")
message("bundleutils: PRISM: Bundle linked libraries and dependencies verbose: \"${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}\" -cd -of -q -f ${_FIXUP_BUNDLES} ${_VERBOSE_FLAG}\"")
execute_process(
  COMMAND /bin/sh -c
    "${_BUNDLER_COMMAND} -a \"${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}\" -cd -of -q -f ${_FIXUP_BUNDLES} ${_VERBOSE_FLAG}"
    ${_QUIET_FLAG})

message("bundleutils: prism: dylibbundler complected")

function(cosign_cef_helper out_name)


  file(GLOB _CEF_FRAMEWORKS "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/Chromium Embedded Framework.framework/Libraries/*.dylib")
  foreach(_CEF_DEPENDENCY IN LISTS _CEF_FRAMEWORKS)
    if(NOT IS_SYMLINK "${_CEF_DEPENDENCY}")
       message("bundleutils: codesign Chromium Embedded Framework.framework sub dir: ${_CEF_DEPENDENCY}")
      execute_process( COMMAND
          /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options runtime --entitlements "${_CODESIGN_ENTITLEMENTS}/entitlements.plist"
          "${_CEF_DEPENDENCY}")
    endif()
  endforeach()

  message("bundleutils: codesign Chromium Embedded Framework.framework")
  execute_process(
  COMMAND
    /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options runtime --entitlements "${_CODESIGN_ENTITLEMENTS}/entitlements.plist"
    "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/Chromium Embedded Framework.framework"
    )

  message("bundleutils: codesign cef -> ${out_name}")
  set(CEF_HELPER_APP_SUFFIXES ":" " (GPU):.gpu" " (Plugin):.plugin"
                              " (Renderer):.renderer")

  foreach(_SUFFIXES ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" _SUFFIXES ${_SUFFIXES})
    list(GET _SUFFIXES 0 _NAME_SUFFIX)
    list(GET _SUFFIXES 1 _PLIST_SUFFIX)

    set(_HELPER_OUTPUT_NAME "${out_name}${_NAME_SUFFIX}")
    set(_HELPER_ENTITLEMENT_PLIST "entitlements-helper${_PLIST_SUFFIX}.plist")

    execute_process(
      COMMAND
        /usr/bin/codesign --remove-signature
        "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/${out_name}${_NAME_SUFFIX}.app"
        ${_VERBOSE_FLAG} ${_QUIET_FLAG})
    execute_process(
      COMMAND
        /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --deep
        --options runtime --entitlements
        "${_CODESIGN_ENTITLEMENTS}/entitlements-helper${_PLIST_SUFFIX}.plist"
        "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/${out_name}${_NAME_SUFFIX}.app"
        ${_VERBOSE_FLAG} ${_QUIET_FLAG})
  endforeach()
endfunction()

if(EXISTS "${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/__pycache__")
  file(REMOVE_RECURSE
       "${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/__pycache__")
endif()

# Find all dylibs, frameworks and other code elements inside bundle
file(GLOB _DYLIBS "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/*.dylib")
file(GLOB _FRAMEWORKS "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/*.framework")
file(GLOB_RECURSE _QT_PLUGINS "${PRISM_BUNDLE_CONTENTS_PATH}/PlugIns/*.dylib")


file(GLOB _PRISM_EXCUTABLES "${PRISM_BUNDLE_CONTENTS_PATH}/MacOS/*")
foreach(_prism_exe ${_PRISM_EXCUTABLES})
  get_filename_component(_prism_name "${_prism_exe}" NAME_WLE)
  if(NOT ${_prism_name} STREQUAL "PRISMLiveStudio")
    list(APPEND _OTHER_BINARIES "${_prism_exe}")
  endif()
  unset(_prism_name)
endforeach()
unset(_PRISM_EXCUTABLES)


if(EXISTS "${PRISM_BUNDLE_CONTENTS_PATH}/Resources/prism-mac-virtualcam.plugin" )
  list( APPEND _OTHER_BINARIES "${PRISM_BUNDLE_CONTENTS_PATH}/Resources/prism-mac-virtualcam.plugin")
endif()

# Codesign all binaries inside-out
message("bundleutils: PRISM: Codesign dependencies")

foreach(_DEPENDENCY IN LISTS _OTHER_BINARIES _DYLIBS _FRAMEWORKS _OBS_PLUGINS
                             _OBS_SCRIPTING_PLUGINS _QT_PLUGINS)
  if(NOT IS_SYMLINK "${_DEPENDENCY}")
    message("bundleutils: PRISM: Codesign item：${_DEPENDENCY}")
    execute_process(COMMAND /usr/bin/codesign --remove-signature
                            "${_DEPENDENCY}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
    execute_process(
      COMMAND
        /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options runtime --entitlements "${_CODESIGN_ENTITLEMENTS}/entitlements.plist"
        "${_DEPENDENCY}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
  endif()
endforeach()

if(EXISTS "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/Sparkle.framework")
  execute_process(
    COMMAND
      /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --deep --options runtime --entitlements "${_CODESIGN_ENTITLEMENTS}/entitlements.plist"
      "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/Sparkle.framework"
      ${_VERBOSE_FLAG} ${_QUIET_FLAG})
endif()

if(EXISTS "${PRISM_BUNDLE_CONTENTS_PATH}/Frameworks/Chromium Embedded Framework.framework")
  cosign_cef_helper("PRISMLiveStudio Helper")
endif()

# Codesign main app
message("bundleutils: PRISM: Codesign main app")
execute_process(
  COMMAND
    /usr/bin/codesign --remove-signature
    "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})
execute_process(
  COMMAND
    /usr/bin/codesign --force --sign "${_CODESIGN_IDENTITY}" --options runtime
    --entitlements "${_CODESIGN_ENTITLEMENTS}/entitlements_vr.plist"
    "${CMAKE_INSTALL_PREFIX}/${_BUNDLENAME}" ${_VERBOSE_FLAG} ${_QUIET_FLAG})

message("bundleutils: PRISM: Codesign main app complected")
