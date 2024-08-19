cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME test_NAME)

#set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO $ENV{PROJECT_DIR}/bin/prism/windows/$ENV{BUILD_TYPE_ARG}/bin/64bit)

project(${test_NAME})

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

enable_testing()

file(GLOB_RECURSE src-files *.cpp *.h *.hpp)
add_executable(${test_NAME} ${src-files})
target_link_libraries(${test_NAME} PRIVATE Qt6::Widgets Qt6::Test)
set_target_properties(${test_NAME} PROPERTIES FOLDER "tests")
copy_prism_common_binary(${test_NAME})
deploy_qt(${test_NAME})

#include(GoogleTest)
add_test(NAME ${test_NAME} COMMAND $ENV{PROJECT_DIR}/bin/prism/windows/$ENV{BUILD_TYPE_ARG}/bin/64bit/$<TARGET_FILE_NAME:${test_NAME}>)
