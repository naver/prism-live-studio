project(PrismCommandLine)

message(STATUS "Sub: PrismCommandLine")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_executable(PrismCommandLine main.cpp)

set_target_properties(PrismCommandLine PROPERTIES FOLDER common-libs/libdump-analyzer)

common_libs_module(PrismCommandLine "common libs comandLine app")

if(OS_WINDOWS) 
copy_common_libs_binary(PrismCommandLine)
elseif(OS_MACOS)
copy_prism_binary(PrismCommandLine)
endif()
