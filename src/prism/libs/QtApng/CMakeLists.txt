cmake_minimum_required(VERSION 3.1.0)

project(QtApng)

add_executable(QtApng qtapng.cpp)
string(REGEX REPLACE "/VC/[a-zA-Z0-9/._ ]*$" "/VC/Auxiliary/Build/vcvars64.bat" VCVARS64 ${CMAKE_CXX_COMPILER})
string(REPLACE "/" "\\" OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS VCVARS64=${VCVARS64})
message(STATUS OUTPUT_DIR=${OUTPUT_DIR})

add_custom_command(TARGET QtApng
	POST_BUILD
	COMMAND "$ENV{ComSpec}"
		"/c"
		"${CMAKE_CURRENT_SOURCE_DIR}/build.cmd"
		"${VCVARS64}"
		"${OUTPUT_DIR}"
		"$<CONFIGURATION>"
		"$ENV{PRISM_DIR}"
		"$ENV{BIN_DIR}"
		"$ENV{ARCH}"
		"$ENV{QTDIR}")
