project(aja-output-ui)

find_package(LibAJANTV2 REQUIRED)

add_library(aja-output-ui MODULE)
add_library(OBS::aja-output-ui ALIAS aja-output-ui)

find_package(Qt6 COMPONENTS Widgets Gui)

set_target_properties(
  aja-output-ui
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "forms")

target_sources(aja-output-ui PRIVATE forms/output.ui)

target_sources(
  aja-output-ui
  PRIVATE AJAOutputUI.h
          AJAOutputUI.cpp
          aja-ui-main.cpp
          aja-ui-main.h
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-card-manager.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-card-manager.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-common.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-common.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-enums.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-presets.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-presets.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-props.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-props.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-routing.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-routing.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-ui-props.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-vpid-data.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-vpid-data.hpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-widget-io.cpp
          $ENV{OBS_SRC_DIR}/plugins/aja/aja-widget-io.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.moc.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.h)

target_link_libraries(aja-output-ui PRIVATE OBS::libobs OBS::frontend-api
                                            Qt6::Widgets AJA::LibAJANTV2
                                            common-libs::ui
                                            prism::frontend-api)
                                            
if(OS_WINDOWS)
target_link_libraries(aja-output-ui PRIVATE OBS::w32-pthreads)
endif()

if(OS_MACOS)
  # cmake-format: off
  # cmake-format: on
  target_compile_options(aja-output-ui PRIVATE -Wno-error=deprecated-declarations
                                               -Wno-quoted-include-in-framework-header)
elseif(OS_WINDOWS)
  target_compile_options(aja-output-ui PRIVATE /wd4996)
  target_link_options(aja-output-ui PRIVATE "LINKER:/IGNORE:4099")
  set_property(
    TARGET aja-output-ui
    APPEND
    PROPERTY AUTORCC_OPTIONS --format-version 1)
else()
  find_package(X11 REQUIRED)
  target_link_libraries(aja-output-ui PRIVATE X11::X11 Qt::GuiPrivate)
endif()

prism_module(aja-output-ui "PRISM AJA Output UI module")
set_target_properties(aja-output-ui PROPERTIES FOLDER "prism/plugins/frontend-plugins")
target_include_directories(aja-output-ui
    PRIVATE $ENV{OBS_SRC_DIR}
            ${PRISMLiveStudio_SOURCE_DIR}/obs-ui
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/main)

get_target_property(_SOURCES aja-output-ui SOURCES)
set(_UI ${_SOURCES})
list(FILTER _UI INCLUDE REGEX ".*\\.ui?")

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/forms"
  PREFIX "UI Files"
  FILES ${_UI})
unset(_SOURCES)
unset(_UI)

copy_prism_plugin_binary(aja-output-ui)




