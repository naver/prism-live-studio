project(libresource)

find_package(Qt6Core REQUIRED)

add_definitions(-DPLSRESOURCEHANDLE_LIBRARY -DLIRESOURCE_LIB -D_CRT_SECURE_NO_WARNINGS)
include_directories(${Qt6Core_PRIVATE_INCLUDE_DIRS})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_AUTOMOC TRUE)

if(OS_WINDOWS)
file(GLOB_RECURSE RESDOWNLOAD-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE RESDOWNLOAD-HDR_FILES *.h *.hpp *.hxx *.hh)
else()
file(GLOB RESDOWNLOAD-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB RESDOWNLOAD-HDR_FILES *.h *.hpp *.hxx *.hh)
endif()

add_library(libresource SHARED ${RESDOWNLOAD-SRC_FILES}
	${RESDOWNLOAD-HDR_FILES}
	)

add_library(common-libs::resource ALIAS libresource)

target_link_libraries(libresource
	Qt6::Core
	common-libs::log
	common-libs::utils-api
	common-libs::http-client
	common-libs::ui)
set_target_properties(libresource PROPERTIES FOLDER common-libs)
set_target_properties(libresource PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libresource "common libs resource module")
copy_common_libs_binary(libresource)
