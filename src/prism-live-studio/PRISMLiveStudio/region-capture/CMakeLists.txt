project(region-capture)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTORCC TRUE)

if(OS_WINDOWS)
	set(MODULE_DESCRIPTION "${PROJECT_NAME}")
	configure_file("${EXE_RC_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc")
	set(MODULE_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc")
elseif(OS_MACOS)
	#TODO
endif()

if(OS_WINDOWS)
add_executable(region-capture
main.cpp 
region-capture.h 
region-capture.cpp
region-capture-service.h
region-capture-service.cpp
win-signal.h 
win-signal.cpp
PLSHookKeyboard.h
PLSHookKeyboard-win.cpp
region-capture.qrc
"${MODULE_RC_FILE}")
elseif(OS_MACOS)
add_executable(region-capture
main.cpp 
region-capture.h 
region-capture.cpp
region-capture-service.h
region-capture-service.cpp
PLSHookKeyboard.h
PLSHookKeyboard-mac.mm
region-capture.qrc)
endif()

find_package(Qt6 COMPONENTS Widgets Network)

target_link_libraries(region-capture OBS::libobs common-libs::utils-api common-libs::ui Qt6::Widgets Qt6::Network)
add_dependencies(region-capture obs-build)

set_target_properties(region-capture PROPERTIES FOLDER prism/frontend)
set_target_properties(region-capture PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

copy_prism_binary(region-capture)
