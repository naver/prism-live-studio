project(libbrowser)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_definitions(-DLIBBROWSER_LIB -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_library(libbrowser SHARED libbrowser.cpp libbrowser.h)
add_library(common::browser ALIAS libbrowser)
add_library(prism::common::browser ALIAS libbrowser)
target_link_libraries(libbrowser Qt6::Widgets OBS::libobs OBS::browser-panels libutils-api liblog libipc libui)
set_target_properties(libbrowser PROPERTIES FOLDER prism/common)
set_target_properties(libbrowser PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

prism_module(libbrowser "PRISM browser module")

copy_prism_common_binary(libbrowser)

