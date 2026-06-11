project(libui)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_definitions(-DLIBUI_LIB -D_CRT_SECURE_NO_WARNINGS)
include_directories(${Qt6Widgets_PRIVATE_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB_RECURSE css_files *.css *.qss)
if(OS_MACOS)
file(GLOB_RECURSE libui-SRC_FILES *.c *.cpp *.cxx *.cc *.mm)
file(GLOB_RECURSE SOURCES_MM_FILES *.mm *.m)
set_source_files_properties(${SOURCES_MM_FILES} PROPERTIES COMPILE_FLAGS -fobjc-arc)
elseif(OS_WINDOWS)
file(GLOB_RECURSE libui-SRC_FILES *.c *.cpp *.cxx *.cc)
endif()
file(GLOB_RECURSE libui-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE libui-UI_FILES *.ui)
file(GLOB_RECURSE libui-QRC_FILES *.qrc)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Css Files" FILES ${css_files})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "UI Files" FILES ${libui-UI_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "RC Files" FILES ${libui-QRC_FILES})

add_library(libui SHARED ${libui-SRC_FILES} ${libui-HDR_FILES} ${libui-UI_FILES} ${libui-QRC_FILES} ${css_files})
add_library(common-libs::ui ALIAS libui)

if(OS_MACOS)
target_compile_options(libui PRIVATE -fobjc-arc)
endif()

target_link_libraries(libui Qt6::Widgets Qt6::Svg Qt6::GuiPrivate common-libs::utils-api common-libs::log common-libs::third-party::QQREncoder)

set_target_properties(libui PROPERTIES FOLDER common-libs)
set_target_properties(libui PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

copy_common_libs_binary(libui)
if(OS_WINDOWS)
common_libs_module(libui "common libs ui components module")
copy_common_libs_data(libui data/locale data/prism-studio/locale/libui)
deploy_qt(libui)
elseif(OS_MACOS)
copy_common_libs_data(libui data/locale data/prism-studio/locale/libui)
endif()
