project(libutils-api)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)
qt_standard_project_setup()

add_definitions(-DLIBUTILSAPI_LIB -D_CRT_SECURE_NO_WARNINGS)
include_directories(${Qt6Core_PRIVATE_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

file(GLOB ALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.h *.hpp *.mm *.m)

if(OS_MACOS)
    find_library(MacNetwork Network)
    find_library(MacSecurity Security)
    find_library(APPKIT Appkit)
    list(REMOVE_ITEM ALL_FILES network-state.h network-state.cpp get_win_ver.cpp)
    file(GLOB_RECURSE SOURCES_MM_FILES *.mm *.m)
    set_source_files_properties(${SOURCES_MM_FILES} PROPERTIES COMPILE_FLAGS -fobjc-arc)
elseif(OS_WINDOWS)
    list(REMOVE_ITEM ALL_FILES network-state-mac.h network-state-mac.mm libutils-api-mac.h libutils-api-mac.mm PLSMacFunction.h PLSMacFunction.m)
endif()

add_library(libutils-api SHARED ${ALL_FILES})
add_library(common-libs::utils-api ALIAS libutils-api)

if(OS_MACOS)
target_link_libraries(libutils-api Qt6::Core Qt6::Gui Qt6::Network ${MacNetwork} ${MacSecurity} ${APPKIT})
target_compile_options(libutils-api PRIVATE -fobjc-arc)
elseif(OS_WINDOWS)
target_link_libraries(libutils-api Qt6::Core Qt6::Gui Qt6::Network)
endif()

set_target_properties(libutils-api PROPERTIES FOLDER common-libs)
set_target_properties(libutils-api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libutils-api "common libs utility API module")
copy_common_libs_binary(libutils-api)
if(OS_WINDOWS)


endif()