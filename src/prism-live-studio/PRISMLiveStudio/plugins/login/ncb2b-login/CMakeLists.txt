project(ncb2b-login)

message(STATUS "PRISM Login Plugin: ncb2b-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE ncb2b-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE ncb2b-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE ncb2b-login-UI_FILES *.ui)
file(GLOB_RECURSE ncb2b-login-QRC_FILES *.qrc)

add_library(ncb2b-login MODULE
 	${ncb2b-login-SRC_FILES} 
 	${ncb2b-login-HDR_FILES} 
 	${ncb2b-login-UI_HEADERS} 
 	${ncb2b-login-QRC_FILES}
 )
prism_module(ncb2b-login "PRISM twitch login module")
target_link_libraries(ncb2b-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(ncb2b-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(ncb2b-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(ncb2b-login)

