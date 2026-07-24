project(youtube-login)

message(STATUS "PRISM Login Plugin: youtube-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE youtube-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE youtube-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE youtube-login-UI_FILES *.ui)
file(GLOB_RECURSE youtube-login-QRC_FILES *.qrc)

add_library(youtube-login MODULE
 	${youtube-login-SRC_FILES} 
 	${youtube-login-HDR_FILES} 
 	${youtube-login-UI_HEADERS} 
 	${youtube-login-QRC_FILES}
 )
prism_module(youtube-login "PRISM youtube login module")
target_link_libraries(youtube-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(youtube-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(youtube-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(youtube-login)