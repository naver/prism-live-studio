project(decklink-captions)

add_library(decklink-captions MODULE)
add_library(OBS::decklink-captions ALIAS decklink-captions)

find_package(Qt6 COMPONENTS Widgets)

target_link_libraries(decklink-captions PRIVATE Qt6::Widgets)

set_target_properties(
  decklink-captions
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
             AUTOUIC_SEARCH_PATHS "forms")

target_compile_features(decklink-captions PRIVATE cxx_std_17)

target_sources(decklink-captions PRIVATE forms/captions.ui)

target_sources(decklink-captions PRIVATE decklink-captions.cpp
                                         decklink-captions.h)

target_link_libraries(decklink-captions PRIVATE OBS::frontend-api OBS::libobs libui)

if(OS_MACOS)
  target_compile_options(decklink-captions PRIVATE -Wno-quoted-include-in-framework-header -Wno-comma)
  target_link_libraries(decklink-captions PRIVATE "$<LINK_LIBRARY:FRAMEWORK,Cocoa.framework>")
elseif(OS_POSIX)
  find_package(X11 REQUIRED)
  mark_as_advanced(X11)
  target_link_libraries(decklink-captions PRIVATE X11::X11)
elseif(OS_WINDOWS)
  set_property(
    TARGET decklink-captions
    APPEND
    PROPERTY AUTORCC_OPTIONS --format-version 1)

endif()

prism_module(decklink-captions "PRISM DeckLink Captions module")
set_target_properties(decklink-captions PROPERTIES FOLDER "prism/plugins/frontend-plugins")

get_target_property(_SOURCES decklink-captions SOURCES)
set(_UI ${_SOURCES})
list(FILTER _UI INCLUDE REGEX ".*\\.ui?")

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/forms"
  PREFIX "UI Files"
  FILES ${_UI})
unset(_SOURCES)
unset(_UI)

copy_prism_plugin_binary(decklink-captions)


