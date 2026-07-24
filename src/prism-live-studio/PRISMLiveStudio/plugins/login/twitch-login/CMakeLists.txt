project(twitch-login)

message(STATUS "PRISM Login Plugin: twitch-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE twitch-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE twitch-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE twitch-login-UI_FILES *.ui)
file(GLOB_RECURSE twitch-login-QRC_FILES *.qrc)

add_library(twitch-login MODULE
 	${twitch-login-SRC_FILES} 
 	${twitch-login-HDR_FILES} 
 	${twitch-login-UI_HEADERS} 
 	${twitch-login-QRC_FILES}
 )
prism_module(twitch-login "PRISM twitch login module")
target_link_libraries(twitch-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(twitch-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(twitch-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(twitch-login)

