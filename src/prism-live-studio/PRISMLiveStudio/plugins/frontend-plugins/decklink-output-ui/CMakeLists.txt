project(decklink-output-ui)

add_library(decklink-output-ui MODULE)
add_library(OBS::decklink-output-ui ALIAS decklink-output-ui)

find_package(Qt6 COMPONENTS Widgets Gui)

set_target_properties(
  decklink-output-ui
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "forms")

target_sources(decklink-output-ui PRIVATE forms/output.ui)

target_sources(
  decklink-output-ui
  PRIVATE DecklinkOutputUI.cpp
          DecklinkOutputUI.h
          decklink-ui-main.cpp
          decklink-ui-main.h
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.moc.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.h)

target_link_libraries(decklink-output-ui PRIVATE OBS::libobs OBS::frontend-api
                                                 Qt6::Widgets common-libs::ui
                                                 prism::frontend-api)
if(OS_WINDOWS)
target_link_libraries(decklink-output-ui PRIVATE OBS::w32-pthreads)
endif()

target_compile_features(decklink-output-ui PRIVATE cxx_std_17)

prism_module(decklink-output-ui "PRISM Decklink Output UI module")
set_target_properties(decklink-output-ui PROPERTIES FOLDER "prism/plugins/frontend-plugins")
target_include_directories(decklink-output-ui
    PRIVATE $ENV{OBS_SRC_DIR}
            ${PRISMLiveStudio_SOURCE_DIR}/obs-ui
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/main)

if(OS_WINDOWS)
  set_property(
    TARGET decklink-output-ui
    APPEND
    PROPERTY AUTORCC_OPTIONS --format-version 1)

elseif(OS_MACOS)
  find_library(COCOA Cocoa)
  mark_as_advanced(COCOA)

  target_link_libraries(decklink-output-ui PRIVATE ${COCOA})
  target_compile_options(decklink-output-ui PRIVATE -Wno-quoted-include-in-framework-header)
elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  target_link_libraries(decklink-output-ui PRIVATE X11::X11 Qt6::GuiPrivate)
endif()

get_target_property(_SOURCES decklink-output-ui SOURCES)
set(_UI ${_SOURCES})
list(FILTER _UI INCLUDE REGEX ".*\\.ui?")

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/forms"
  PREFIX "UI Files"
  FILES ${_UI})
unset(_SOURCES)
unset(_UI)

copy_prism_plugin_binary(decklink-output-ui)

