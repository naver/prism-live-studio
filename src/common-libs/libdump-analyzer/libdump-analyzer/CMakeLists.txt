project(libdump-analyzer)

find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

add_definitions(-DLIBDUMPANALUZER_LIB -D_CRT_SECURE_NO_WARNINGS)
include_directories(${Qt6Core_PRIVATE_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)

add_library(libdump-analyzer SHARED)
add_library(common-libs::dump-analyzer ALIAS libdump-analyzer)

if(OS_WINDOWS)
	target_sources(
		libdump-analyzer
		PRIVATE PLSUtil.cpp
		PLSUtil.h
		PLSProcessInfo.h
		PLSDumpAnalyzer.h
		PLSDumpAnalyzer.cpp
		PLSAnalysisStack.h
		PLSAnalysisStack.cpp
		windows/PLSParseDumpFile.cpp
		windows/PLSParseDumpFile.h
		windows/PLSSoftStatistic.cpp
		windows/PLSSoftStatistic.h
	)
elseif(OS_MACOS)
	target_compile_options(libdump-analyzer PRIVATE -fobjc-arc)
	target_sources(
		libdump-analyzer
		PRIVATE PLSUtil.cpp
		PLSUtil.h
		mac/PLSUtilInterface.h
		mac/PLSUtil.mm
		PLSProcessInfo.h
		PLSDumpAnalyzer.h
		PLSDumpAnalyzer.cpp
		PLSAnalysisStack.h
		PLSAnalysisStack.cpp
		mac/PLSAnalysisStackInterface.h
		mac/PLSAnalysisStack.mm
		mac/mach_backtrace.h
		mac/mach_backtrace.c)

	target_link_libraries(libdump-analyzer common-libs::third-party::kscrash)
endif()

target_link_libraries(libdump-analyzer Qt6::Core common-libs::utils-api common-libs::http-client)
set_target_properties(libdump-analyzer PROPERTIES FOLDER common-libs/libdump-analyzer)
set_target_properties(libdump-analyzer PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

common_libs_module(libdump-analyzer "common libs dump analyzer module")
copy_common_libs_binary(libdump-analyzer)
