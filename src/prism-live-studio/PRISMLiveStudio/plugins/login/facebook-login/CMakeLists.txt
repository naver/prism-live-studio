project(facebook-login)

message(STATUS "PRISM Login Plugin: facebook-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE facebook-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE facebook-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE facebook-login-UI_FILES *.ui)
file(GLOB_RECURSE facebook-login-QRC_FILES *.qrc)

add_library(facebook-login MODULE
 	${facebook-login-SRC_FILES} 
 	${facebook-login-HDR_FILES} 
 	${facebook-login-UI_HEADERS} 
 	${facebook-login-QRC_FILES}
 )
prism_module(facebook-login "PRISM facebook login module")
target_link_libraries(facebook-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(facebook-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(facebook-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(facebook-login)