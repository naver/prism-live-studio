project(libipc)

find_package(Qt6Core REQUIRED)
find_package(Qt6Network REQUIRED)

add_definitions(-DLIBIPC_LIB -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

add_library(libipc SHARED libipc.cpp libipc.h)
add_library(common-libs::ipc ALIAS libipc)
target_link_libraries(libipc Qt6::Core Qt6::Network common-libs::utils-api common-libs::http-client common-libs::log)
set_target_properties(libipc PROPERTIES FOLDER common-libs)
set_target_properties(libipc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libipc "common libs ipc module")
copy_common_libs_binary(libipc)
