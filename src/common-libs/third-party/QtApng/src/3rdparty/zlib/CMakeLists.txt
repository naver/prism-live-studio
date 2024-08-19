project(qtapng_zlib)

file(GLOB files src/*.h src/*.c)

add_library(qtapng_zlib STATIC ${files})
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_definitions(qtapng_zlib PRIVATE -DHAVE_UNISTD_H)
endif()

set_target_properties(qtapng_zlib PROPERTIES FOLDER common-libs/third-party/qapng)
set_target_properties(
	qtapng_zlib PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
