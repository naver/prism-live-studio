project(PRISMDaemon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_executable(${PROJECT_NAME} main.cpp)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(${PROJECT_NAME} Qt6::Core common-libs::utils-api common-libs::log)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER common-libs/PRISMDaemon)

common_libs_exe(${PROJECT_NAME} "common PRISMDaemon exe")
if(OS_WINDOWS)
copy_common_libs_binary(${PROJECT_NAME})
elseif(OS_MACOS)
copy_excutable_target(${PROJECT_NAME})
endif()
