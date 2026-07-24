project(chzzk-login)

message(STATUS "PRISM Login Plugin: chzzk-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE chzzk-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE chzzk-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE chzzk-login-UI_FILES *.ui)
file(GLOB_RECURSE chzzk-login-QRC_FILES *.qrc)

add_library(chzzk-login MODULE
 	${chzzk-login-SRC_FILES} 
 	${chzzk-login-HDR_FILES} 
 	${chzzk-login-UI_HEADERS} 
 	${chzzk-login-QRC_FILES}
 )
prism_module(chzzk-login "PRISM chzzk login module")
target_link_libraries(chzzk-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(chzzk-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(chzzk-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(chzzk-login)