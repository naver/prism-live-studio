project(libhttp-client)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_definitions(-DLIBHTTPCLIENT_LIB -D_CRT_SECURE_NO_WARNINGS)
include_directories(${libutils-api_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

add_library(libhttp-client SHARED libhttp-client.cpp libhttp-client.h)
add_library(common-libs::http-client ALIAS libhttp-client)


target_link_libraries(libhttp-client Qt6::Core Qt6::Network common-libs::utils-api common-libs::log)

set_target_properties(libhttp-client PROPERTIES FOLDER common-libs)
set_target_properties(libhttp-client PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libhttp-client "common libs http client module")
copy_common_libs_binary(libhttp-client)
if(OS_WINDOWS)
copy_third_party_openssl(libhttp-client)
endif()
