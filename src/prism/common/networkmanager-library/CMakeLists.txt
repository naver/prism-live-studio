if (NOT WIN32)
	return()
endif()

message(STATUS "PRISM Common Library: networkmanager-library")

project(networkmanager-library)

set(CMAKE_AUTOMOC TRUE)

find_package(Qt5Network )
find_package(Qt5Core)

file(GLOB_RECURSE src_files *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE hdr_files *.h *.hpp *.hxx *.hh)

include_directories(
	"$ENV{PRISM_DIR}/libs/HMAC/include"
	"$ENV{PRISM_DIR}/main"
	)

add_library(networkmanager-library SHARED ${src_files} ${hdr_files})

add_definitions(-DNETWORKACCESSMANAGER_LIBRARY)

target_link_libraries( networkmanager-library
    gdiplus
    Qt5::Network
    Qt5::Core
    frontend-api
    )
add_obs_dependency(networkmanager-library)
link_obs(networkmanager-library)
link_hmac(networkmanager-library)

install_obs_core(networkmanager-library)
copy_prism_target(networkmanager-library)
