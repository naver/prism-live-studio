include(${CMAKE_SOURCE_DIR}/prism-live-studio/cmake/disable_api.cmake)

add_subdirectory(third-party)

InsertDisabledWinAPI() # should skip third-party

add_subdirectory(libutils-api)
add_subdirectory(liblog)
add_subdirectory(libhttp-client)
add_subdirectory(libipc)
add_subdirectory(libtask)
add_subdirectory(libui)
add_subdirectory(libresource)
add_subdirectory(libdump-analyzer)
add_subdirectory(daemon)
if(OS_WINDOWS)
add_subdirectory(util-pc-detect)
endif()