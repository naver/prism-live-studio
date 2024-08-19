
message(STATUS "PRISM Plugin: prism-chzzk-sponsor-source")

project(prism-chzzk-sponsor-source)

add_definitions(-D_PRISM_PLUGIN)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

file(GLOB SRC_FILES *.cpp *.h *.qrc)

add_library(${PROJECT_NAME} MODULE ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
	common-libs::utils-api
	common-libs::log
	common-libs::ui
	prism::common::log
	prism::frontend-api
	Qt6::Widgets)
if(OS_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE OBS::w32-pthreads)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "prism/plugins/source")

copy_prism_plugin_binary(${PROJECT_NAME})
