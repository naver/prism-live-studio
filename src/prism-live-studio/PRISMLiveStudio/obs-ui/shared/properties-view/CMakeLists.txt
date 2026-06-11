cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Core Widgets)

add_library(properties-view INTERFACE)
add_library(OBS::properties-view ALIAS properties-view)

target_sources(
  properties-view
  INTERFACE
    double-slider.cpp
    double-slider.hpp
    properties-view.cpp
    properties-view.hpp
    properties-view.moc.hpp
    spinbox-ignorewheel.cpp
    spinbox-ignorewheel.hpp
)
target_include_directories(properties-view INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
  properties-view
  INTERFACE
    OBS::frontend-api
    OBS::libobs
    OBS::qt-wrappers
    OBS::qt-plain-text-edit
    OBS::qt-vertical-scroll-area
    OBS::qt-slider-ignorewheel
    OBS::qt-icon-label
    Qt::Core
    Qt::Widgets
    prism::frontend-api
)
