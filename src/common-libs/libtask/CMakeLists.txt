project(libtask)

find_package(Qt6Core REQUIRED)
find_package(Qt6Network REQUIRED)

add_definitions(-DLIBTASK_LIB -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

add_library(libtask SHARED libtask.cpp libtask.h)
add_library(common-libs::task ALIAS libtask)
target_link_libraries(libtask Qt6::Core common-libs::ipc common-libs::log common-libs::utils-api)
set_target_properties(libtask PROPERTIES FOLDER common-libs)
set_target_properties(libtask PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libtask "common libs task module")
copy_common_libs_binary(libtask)
