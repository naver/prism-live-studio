project(navertv-login)

message(STATUS "PRISM Login Plugin: navertv-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE navertv-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE navertv-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE navertv-login-UI_FILES *.ui)
file(GLOB_RECURSE navertv-login-QRC_FILES *.qrc)

add_library(navertv-login MODULE
 	${navertv-login-SRC_FILES} 
 	${navertv-login-HDR_FILES} 
 	${navertv-login-UI_HEADERS} 
 	${navertv-login-QRC_FILES}
 )
prism_module(navertv-login "PRISM navertv login module")
target_link_libraries(navertv-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(navertv-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(navertv-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(navertv-login)