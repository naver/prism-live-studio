cmake_minimum_required(VERSION 3.20)

# Enable modern cmake policies
if(POLICY CMP0009)
  cmake_policy(SET CMP0009 NEW)
endif()

if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif()

if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# Build options
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES
      "${CMAKE_HOST_SYSTEM_PROCESSOR}"
      CACHE STRING
            "prism build architecture for macOS - x86_64 required at least" FORCE)
endif()
set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS arm64 x86_64
                                                    "x86_64;arm64")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE STRING "Directory to install prism after building" FORCE)
endif()

if(NOT DEFINED CMAKE_PREFIX_PATH)
  message(
    WARNING
      "No CMAKE_PREFIX_PATH set: OBS supplies pre-built dependencies for building on macOS.\n"
      "While OBS can be built using packages installed via Homebrew, pre-built dependencies "
      "contain beneficial patches and fixes for use within OBS and is the suggested source "
      "of these dependencies.\n"
      "You can download the appropriate obs-deps package for your "
      "architecture and set CMAKE_PREFIX_PATH to this directory:\n"
      "CMAKE_PREFIX_PATH=\"<PATH_TO_OBS_DEPS>\"\n"
      "Download pre-built OBS dependencies at https://github.com/obsproject/obs-deps/releases\n"
  )
endif()

# SWIG hard codes the directory to its library directory at compile time. As
# obs-deps need to be relocatable, we need to force SWIG to look for its files
# in a directory relative to the PREFIX_PATH. The best way to ensure this is to
# set the SWIG_LIB environment variable.

if(NOT DEFINED ENV{SWIG_LIB} AND EXISTS "${CMAKE_PREFIX_PATH}/bin/swig")
  set(ENV{SWIG_LIB} "${CMAKE_PREFIX_PATH}/share/swig/CURRENT")
endif()

macro(setup_prism_project)
  set(CMAKE_XCODE_GENERATE_SCHEME ON)

  message("CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf-with-dsym")

  message("setup_prism_project XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY: ${PRISM_BUNDLE_CODESIGN_IDENTITY}")
  message("setup_prism_project PRISM_BUNDLE_CODESIGN_TEAM: ${PRISM_BUNDLE_CODESIGN_TEAM}")

  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${PRISM_BUNDLE_CODESIGN_IDENTITY}")
  set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${OBS_BUNDLE_CODESIGN_TEAM}")
  
  set(MACOSX_EXCUTABLE_UIElement "1" CACHE INTERNAL "show or hide dock icon")

  set(OBS_CODESIGN_ENTITLEMENTS
      "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/entitlements.plist"
      CACHE INTERNAL "Path to codesign entitlements plist")
  set(OBS_CODESIGN_LINKER
      ON
      CACHE BOOL "Enable linker code-signing on macOS (macOS 11+ required)")

  # Tell Xcode to pretend the linker signed binaries so that editing with
  # install_name_tool preserves ad-hoc signatures. This option is supported by
  # codesign on macOS 11 or higher. See CMake Issue 21854:
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21854

  if(OBS_CODESIGN_LINKER)
    set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "-o linker-signed --timestamp")
    set(LLVM_EXTERNALIZE_DEBUGINFO ON)
  endif()

  # Set default options for bundling on macOS
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_SKIP_BUILD_RPATH OFF)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks/")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

  # Set bundle parameters for cmake's automatic plist generation
  set(MACOSX_BUNDLE_EXECUTABLE_NAME "PRISMLiveStudio")
  set(MACOSX_BUNDLE_BUNDLE_NAME "PRISMLiveStudio")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${OBS_BUILD_NUMBER}")
  set(MACOSX_BUNDLE_COPYRIGHT "${OBS_LEGAL_COPYRIGHT}")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER $ENV{PRISM_PRODUCT_IDENTIFIER})
  set(MACOSX_BUNDLE_ICON_FILE "AppIcon")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${OBS_VERSION_CANONICAL}")
  string(TIMESTAMP CURRENT_YEAR "%Y")

  # Set paths for distribution bundling
  set(OBS_BUNDLE_NAME "PRISMLiveStudio")
  set(OBS_EXECUTABLE_DESTINATION "${CMAKE_INSTALL_BINDIR}")
  set(OBS_INCLUDE_DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/prism")
  set(OBS_LIBRARY_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  set(OBS_CMAKE_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake")

  if(BUILD_FOR_DISTRIBUTION)
    # set_option(CMAKE_BUILD_TYPE "Release")
    set(CPACK_PACKAGE_VERSION "${OBS_VERSION_CANONICAL}")
  else()
    set(CPACK_PACKAGE_VERSION "${OBS_VERSION_CANONICAL}.${OBS_BUILD_NUMBER}")
  endif()

  set(CPACK_PACKAGE_NAME "PRISMLiveStudio")
  # set(CPACK_PACKAGE_VENDOR "${OBS_WEBSITE}")
  set(CPACK_PACKAGE_VENDOR "https://prismlive.com/en_us/pcapp/")
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${OBS_COMMENTS}")

  # if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
  #   set(CPACK_ARCH_SUFFIX "Intel")
  # elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  #   set(CPACK_ARCH_SUFFIX "Apple")
  # else()
  #   set(CPACK_ARCH_SUFFIX "Universal")
  # endif()

  # set(CPACK_PACKAGE_FILE_NAME
  #     "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-macOS-${CPACK_ARCH_SUFFIX}"
  # )
  set(CPACK_PACKAGE_FILE_NAME "PRISMLiveStudio_${CPACK_PACKAGE_VERSION}")

  set(CPACK_COMPONENTS_ALL prism_app prism_plugins prism_frameworks 
                           prism_libraries prism_install_obs prism_lancher prism_bundles)

  set(CPACK_COMPONENT_PRISM_APP_DISPLAY_NAME "PRISM Live Studio")
  set(CPACK_COMPONENT_PRISM_PLUGINS_DISPLAY_NAME "PRISM Plugins")
  set(CPACK_COMPONENT_PRISM_FRAMEWORKS_DISPLAY_NAME "PRISM Frameworks")
  set(CPACK_COMPONENT_PRISM_LIBRARIES_DISPLAY_NAME "PRISM Libraries")
  set(CPACK_COMPONENT_PRISM_INSTALL_OBS_DISPLAY_NAME "PRISM Install OBS")
  set(CPACK_COMPONENT_PRISM_INSTALL_OBS_DISPLAY_NAME "PRISM Install OBS")
  set(CPACK_COMPONENT_PRISM_BUNDLES_DISPLAY_NAME "PRISM Bundle")


  set(CPACK_DMG_BACKGROUND_IMAGE
      "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/background.tiff")
  set(CPACK_PACKAGE_ICON "$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/AppIcon.icns")
  get_filename_component(CPACK_DMG_BACKGROUND_FILENAME
                         ${CPACK_DMG_BACKGROUND_IMAGE} NAME)
  set(CPACK_DMG_FORMAT "UDZO")
  set(CPACK_DMG_FILESYSTEM "APFS")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_BINARY_DIR}/package.applescript")

  set(_DMG_WINDOW_X "100")
  set(_DMG_WINDOW_Y "100")
  set(_DMG_WINDOW_WIDTH "510")
  set(_DMG_WINDOW_HEIGHT "340")
  set(_DMG_ICON_SIZE "130")
  set(_DMG_TEXT_SIZE "13")
  set(_DMG_OBS_X "131")
  set(_DMG_OBS_Y "117")
  set(_DMG_APP_LINK_X "372")
  set(_DMG_APP_LINK_Y "117")

  configure_file("$ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/package.applescript.in"
                 "${CMAKE_BINARY_DIR}/package.applescript" @ONLY)

  include(CPack)

  install(
    CODE "
    set(_BUNDLENAME \"$<TARGET_FILE_BASE_NAME:PRISMLiveStudio>.app\")
    set(_OBSAPPPATH \"\$ENV{OBS_BUILD_DIR}/install\")
    if(EXISTS \"\${CMAKE_INSTALL_PREFIX}/\${_BUNDLENAME}\")
      file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/\${_BUNDLENAME}\")
    endif()")

endmacro()
