project(qtapng_png)

file(GLOB files src/*.h src/*.c)
list(REMOVE_ITEM files ${CMAKE_CURRENT_SOURCE_DIR}/src/example.c)

add_library(qtapng_png STATIC ${files})
target_compile_definitions(qtapng_png PRIVATE -DPNG_ARM_NEON_OPT=0)
target_link_directories(qtapng_png PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(qtapng_png qtapng_zlib)
set_target_properties(qtapng_png PROPERTIES FOLDER common-libs/third-party/qapng)
set_target_properties(
	qtapng_png PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")