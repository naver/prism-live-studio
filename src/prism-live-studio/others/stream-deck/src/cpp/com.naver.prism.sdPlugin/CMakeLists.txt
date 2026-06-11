project(stream-deck)

cmake_minimum_required(VERSION 3.24)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DASIO_STANDALONE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

if(MSVC)
    #report an updated value for recent C++ language standards support
    add_compile_options(/Zc:__cplusplus)
else()
endif()

if(WIN32)
add_executable(stream-deck
        common/EPLJSONUtils.h
        common/ESDBasePlugin.h
        common/ESDConnectionManager.h
        common/ESDConnectionManager.cpp
        common/ESDLocalizer.h
        common/ESDLocalizer.cpp
        common/ESDSDKDefines.h
        common/ESDUtilities.h
        common/PLSConnectionManager.h
        common/PLSConnectionManager.cpp
        common/PLSPluginBase.h
        common/PLSStreamDeckPlugin.h
        common/PLSStreamDeckPlugin.cpp
        common/main.cpp
        common/ESDUtilitiesWindows.cpp
        windows/pch.h
        windows/pch.cpp)
endif()

if(APPLE)
add_executable(stream-deck
        common/EPLJSONUtils.h
        common/ESDBasePlugin.h
        common/ESDConnectionManager.h
        common/ESDConnectionManager.cpp
        common/ESDLocalizer.h
        common/ESDLocalizer.cpp
        common/ESDSDKDefines.h
        common/ESDUtilities.h
        common/PLSConnectionManager.h
        common/PLSConnectionManager.cpp
        common/PLSPluginBase.h
        common/PLSStreamDeckPlugin.h
        common/PLSStreamDeckPlugin.cpp
        common/main.cpp
        common/ESDUtilitiesMac.cpp
        mac/pch.h)
        find_library(APPKIT CoreFoundation)
endif()

if(APPLE)
find_library(APPKIT Foundation)
target_link_libraries(stream-deck ${APPKIT})
endif()

#Rename the executable file
set_target_properties(stream-deck PROPERTIES OUTPUT_NAME prismPlugin)
if(APPLE)
message(STATUS "APPKIT is ${APPKIT}")
target_link_libraries(stream-deck ${APPKIT})
endif()

target_include_directories(
    stream-deck
    PRIVATE ${CMAKE_SOURCE_DIR}/vendor/asio/include
    ${CMAKE_SOURCE_DIR}/vendor/websocketpp
)

add_custom_command(TARGET stream-deck POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/../../html/com.naver.prism.sdPlugin  ${CMAKE_CURRENT_SOURCE_DIR}/../../../package/com.naver.prism.sdPlugin
)

if(MSVC)
add_custom_command(TARGET stream-deck POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/prismPlugin.exe  ${CMAKE_CURRENT_SOURCE_DIR}/../../../package/com.naver.prism.sdPlugin/prismPlugin.exe
)
elseif(APPLE)
add_custom_command(TARGET stream-deck POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/prismPlugin  ${CMAKE_CURRENT_SOURCE_DIR}/../../../package/com.naver.prism.sdPlugin/prismPlugin
)
endif()
