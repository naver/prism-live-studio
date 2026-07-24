include(${CMAKE_SOURCE_DIR}/prism-live-studio/cmake/disable_api.cmake)

add_subdirectory(third-party)

InsertDisabledWinAPI() # should skip third-party

add_subdirectory(libutils-api)
add_subdirectory(liblog)
add_subdirectory(libhttp-client)
if(PRODUCT_TYPE STREQUAL "PRISM")
	add_subdirectory(libipc)
	add_subdirectory(libtask)
endif()
add_subdirectory(libui)
add_subdirectory(libresource)
add_subdirectory(libdump-analyzer)
add_subdirectory(pls-loading-view)
