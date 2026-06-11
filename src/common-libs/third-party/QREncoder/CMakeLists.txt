cmake_minimum_required(VERSION 3.5)

project(QQREncoder)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DHAVE_CONFIG_H)

find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

set(ENCODE_DIR "./qrencode-4.1.0")
include_directories(${ENCODE_DIR})

FILE(GLOB ENCODE_HEADERS "${ENCODE_DIR}/*.h")


FILE(GLOB ENCODE_SRCS "${ENCODE_DIR}/*.c" "${ENCODE_DIR}/*.cpp")

message( ${ENCODE_SRCS})
add_library(QQREncoder STATIC ${ENCODE_SRCS} ${ENCODE_HEADERS})
add_library(common-libs::third-party::QQREncoder ALIAS QQREncoder)
target_link_libraries(QQREncoder  Qt6::Core Qt6::Widgets)

set_target_properties(QQREncoder PROPERTIES FOLDER common-libs/third-party)
set_target_properties(QQREncoder PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/qrencode-4.1.0")

if(OS_WINDOWS)
copy_common_libs_binary(QQREncoder)
elseif(OS_MACOS)
copy_static_lib_target(QQREncoder)
endif()
