project(afreecatv-login)

message(STATUS "PRISM Login Plugin: afreecatv-login")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)

file(GLOB_RECURSE afreecatv-login-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE afreecatv-login-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE afreecatv-login-UI_FILES *.ui)
file(GLOB_RECURSE afreecatv-login-QRC_FILES *.qrc)

add_library(afreecatv-login MODULE
 	${afreecatv-login-SRC_FILES} 
 	${afreecatv-login-HDR_FILES} 
 	${afreecatv-login-UI_HEADERS} 
 	${afreecatv-login-QRC_FILES}
 )
prism_module(afreecatv-login "PRISM afreecatv login module")
target_link_libraries(afreecatv-login
	Qt6::Widgets
	Qt6::Network
	common-libs::utils-api
	common-libs::http-client
	common::browser
	prism::frontend-api
	)
set_target_properties(afreecatv-login PROPERTIES FOLDER "prism/plugins/login")


if(OS_WINDOWS)
add_dependencies(afreecatv-login PRISMLiveStudio)
endif()

copy_prism_plugin_binary(afreecatv-login)