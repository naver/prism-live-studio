project(PRISMLogger)

message(STATUS "Sub: PRISMLogger")

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_PREFIX_PATH $ENV{QTDIR64})

find_package(Qt5 COMPONENTS Core REQUIRED)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_executable(PRISMLogger
		main.cpp
		send-dump.hpp
		string-convert.h
		string-convert.cpp
		check-sys-dumps.cpp
		ParseDumpFile.cpp)

target_link_libraries(${PROJECT_NAME} Qt5::Core obs)

link_nelo(PRISMLogger)

install_obs_core(PRISMLogger)
copy_prism_target(PRISMLogger)

