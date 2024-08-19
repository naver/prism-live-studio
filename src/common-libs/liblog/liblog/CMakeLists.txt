project(liblog)

find_package(Qt6Core REQUIRED)

add_definitions(-DLIBLOG_LIB -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_library(liblog SHARED liblog.cpp liblog.h)
add_library(common-libs::log ALIAS liblog)
target_link_libraries(liblog Qt6::Core common-libs::utils-api)
set_target_properties(liblog PROPERTIES FOLDER common-libs/liblog)
set_target_properties(liblog PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(liblog "common libs log module")
copy_common_libs_binary(liblog)


