cmake_minimum_required(VERSION 3.1.0)

project(QtApng)

add_executable(QtApng qtapng.cpp)
set_target_properties(QtApng PROPERTIES FOLDER common-libs/third-party)


if(OS_WINDOWS)

string(REGEX REPLACE "/VC/[a-zA-Z0-9/._ ]*$" "/VC/Auxiliary/Build/vcvars64.bat" VCVARS64 ${CMAKE_CXX_COMPILER})
string(REPLACE "/" "\\" OUTPUT_DIR ${QtApng_BINARY_DIR})

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
		"$ENV{PRISM_SRC_DIR}"
		"$ENV{DEV_OUTPUT_DIR}"
		"$ENV{ARCH}"
		"$ENV{QTDIR}")
elseif(OS_MACOS)

# message(STATUS "QtApng IGNORE_APNG_BUILD: $ENV{IGNORE_APNG_BUILD}")
# if($ENV{IGNORE_APNG_BUILD} MATCHES "TRUE")
# 	message(STATUS "QtApng use packaged dylib")
# else()
# 	message(STATUS "QtApng will build by self")
# add_custom_command(TARGET QtApng
# 	PRE_BUILD
# 	COMMAND bash
# 		"${CMAKE_CURRENT_SOURCE_DIR}/buildQtApng.sh"
# 		"${CMAKE_CURRENT_BINARY_DIR}"
# 		"$ENV{QTDIR}"
# 		"$ENV{ARCH}")

# message(STATUS "QtApng output ${CMAKE_CURRENT_BINARY_DIR}")
# endif()

endif()

function(copy_third_party_apng target)
	

if(OS_WINDOWS)
	add_dependencies(${target} QtApng)
	add_custom_command(TARGET ${target}
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}"
			"-DCONFIG=$<CONFIGURATION>"
			"-DQtApng_BINARY_DIR=${QtApng_BINARY_DIR}"
			"-DDEV_OUTPUT_DIR=$ENV{DEV_OUTPUT_DIR}"
			"-DARCH=$ENV{ARCH}"
			"-DTARGET=${target}"
			"-DOS_WINDOWS=${OS_WINDOWS}"
			"-DOS_MACOS=${OS_MACOS}"
			-P "${QtApng_SOURCE_DIR}/copy.cmake"
		VERBATIM)
elseif(OS_MACOS)

# if($ENV{IGNORE_APNG_BUILD} MATCHES "TRUE")
#      add_custom_command(
#         TARGET ${target}
#         POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy "${QtApng_SOURCE_DIR}/packaged_lib/libqapng.dylib" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/PlugIns/imageformats/libqapng.dylib"
#         COMMENT "Installing PRISM libqapng.dylib"
#         VERBATIM)
# else()
#      add_custom_command(
#         TARGET ${target}
#         POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy "${QtApng_BINARY_DIR}/plugins/imageformats/libqapng.dylib" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/PlugIns/imageformats/libqapng.dylib"
#         COMMENT "Installing PRISM libqapng.dylib"
#         VERBATIM)
# endif()

endif()

endfunction()



