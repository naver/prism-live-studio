project(naver-shopping-live-login)

message(STATUS "PRISM Login Plugin: naver-shopping-live-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE naver-shopping-live-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE naver-shopping-live-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE naver-shopping-live-login-UI_FILES *.ui)
file(GLOB_RECURSE naver-shopping-live-login-QRC_FILES *.qrc)

add_library(naver-shopping-live-login MODULE
 	${naver-shopping-live-login-SRC_FILES} 
 	${naver-shopping-live-login-HDR_FILES} 
 	${naver-shopping-live-login-UI_FILES} 
 	${naver-shopping-live-login-QRC_FILES}
 )
prism_module(naver-shopping-live-login "PRISM naver-shopping-live login module")
target_link_libraries(naver-shopping-live-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(naver-shopping-live-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(naver-shopping-live-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(naver-shopping-live-login)