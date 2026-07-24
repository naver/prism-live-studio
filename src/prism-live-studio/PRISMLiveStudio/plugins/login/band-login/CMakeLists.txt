project(band-login)

message(STATUS "PRISM Login Plugin: band-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE band-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE band-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE band-login-UI_FILES *.ui)
file(GLOB_RECURSE band-login-QRC_FILES *.qrc)

add_library(band-login MODULE
 	${band-login-SRC_FILES} 
 	${band-login-HDR_FILES} 
 	${band-login-UI_HEADERS} 
 	${band-login-QRC_FILES}
 )
prism_module(band-login "PRISM band login module")
target_link_libraries(band-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(band-login PROPERTIES FOLDER "prism/plugins/login")

if(OS_WINDOWS)
add_dependencies(band-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(band-login)


