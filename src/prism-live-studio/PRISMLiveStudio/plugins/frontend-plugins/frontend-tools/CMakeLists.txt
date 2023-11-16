project(frontend-tools)

add_library(frontend-tools MODULE)
add_library(OBS::frontend-tools ALIAS frontend-tools)

find_package(Qt6 COMPONENTS Widgets Gui)

set_target_properties(
  frontend-tools
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "forms")

target_sources(
  frontend-tools PRIVATE forms/auto-scene-switcher.ui forms/captions.ui
                         forms/output-timer.ui forms/scripts.ui)

target_sources(
  frontend-tools
  PRIVATE frontend-tools.c
          auto-scene-switcher.hpp
          auto-scene-switcher.cpp
          output-timer.hpp
          tool-helpers.hpp
          output-timer.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/double-slider.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/horizontal-scroll-area.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/horizontal-scroll-area.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/properties-view.moc.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/qt-wrappers.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/spinbox-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/slider-ignorewheel.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/vertical-scroll-area.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/obs-ui/plain-text-edit.hpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.cpp
          ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common/PLSMessageBox.h)

target_compile_features(frontend-tools PRIVATE cxx_std_17)

target_link_libraries(frontend-tools PRIVATE OBS::frontend-api OBS::libobs
                                             Qt6::Widgets
                                             common-libs::ui
                                             prism::frontend-api)
if(OS_WINDOWS)
target_link_libraries(frontend-tools PRIVATE OBS::w32-pthreads)
endif()

if(OS_POSIX AND NOT OS_MACOS)
  target_link_libraries(frontend-tools PRIVATE Qt6::GuiPrivate)
endif()

target_compile_definitions(frontend-tools PRIVATE ENABLE_SCRIPTING)

target_sources(frontend-tools PRIVATE scripts.cpp scripts.hpp)

target_link_libraries(frontend-tools PRIVATE OBS::scripting libui)

target_compile_definitions(frontend-tools PRIVATE LUAJIT_FOUND)

target_compile_definitions(frontend-tools PRIVATE Python_FOUND)

prism_module(frontend-tools "PRISM Frontend Tools module")
set_target_properties(frontend-tools PROPERTIES FOLDER "prism/plugins/frontend-plugins")
target_include_directories(frontend-tools
    PRIVATE ${PRISMLiveStudio_SOURCE_DIR}/obs-ui
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/common
            ${PRISMLiveStudio_SOURCE_DIR}/prism-ui/main)

if(OS_WINDOWS)
  target_sources(
    frontend-tools
    PRIVATE auto-scene-switcher-win.cpp
            captions.cpp
            captions.hpp
            captions-handler.cpp
            captions-handler.hpp
            captions-mssapi.cpp
            captions-mssapi.hpp
            captions-mssapi-stream.cpp
            captions-mssapi-stream.hpp)

elseif(OS_MACOS)
  find_library(COCOA Cocoa)
  mark_as_advanced(COCOA)
  target_link_libraries(frontend-tools PRIVATE ${COCOA})

  target_sources(frontend-tools PRIVATE auto-scene-switcher-osx.mm)
  set_source_files_properties(auto-scene-switcher-osx.mm
                              PROPERTIES COMPILE_FLAGS -fobjc-arc)
  
  setup_plugin_extra_dependency(OBS::scripting)
elseif(OS_POSIX)
  find_package(X11 REQUIRED)

  target_link_libraries(frontend-tools PRIVATE X11::X11)

  target_sources(frontend-tools PRIVATE auto-scene-switcher-nix.cpp)
endif()

get_target_property(_SOURCES frontend-tools SOURCES)
set(_UI ${_SOURCES})
list(FILTER _UI INCLUDE REGEX ".*\\.ui?")

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/forms"
  PREFIX "UI Files"
  FILES ${_UI})
unset(_SOURCES)
unset(_UI)

copy_prism_plugin_binary(frontend-tools)
