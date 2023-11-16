cmake_minimum_required(VERSION 3.0)

project(parallelsign)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6Core REQUIRED)

add_executable(parallelsign main.cpp)
target_link_libraries(parallelsign Qt6::Core)
