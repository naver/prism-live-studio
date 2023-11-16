project(vst-check)

find_package(Qt6Core REQUIRED)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

add_executable(vst-check main.cpp)

target_link_libraries(vst-check OBS::libobs common-libs::utils-api OBS::frontend-api)
add_dependencies(vst-check obs-build)


set_target_properties(vst-check PROPERTIES FOLDER prism/frontend)
set_target_properties(vst-check PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

copy_prism_binary(vst-check)
