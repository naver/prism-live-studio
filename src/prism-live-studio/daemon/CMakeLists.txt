project(PRISMDaemon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

if(OS_WINDOWS)
	add_executable(${PROJECT_NAME} WIN32 main-win.cpp)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        OUTPUT_NAME PRISMLiveStudio
        FOLDER prism/PRISMDaemon
    )
    target_link_libraries(${PROJECT_NAME} common-libs::ui)
elseif(OS_MACOS)
	add_executable(${PROJECT_NAME} main-mac.cpp)
	set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER prism/PRISMDaemon)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(${PROJECT_NAME} Qt6::Core common-libs::utils-api common-libs::log)

common_libs_exe(${PROJECT_NAME} "Live Studio")
if(OS_WINDOWS)
copy_target(${PROJECT_NAME} PRISMLiveStudio "" bin/64bit)
elseif(OS_MACOS)
copy_excutable_target(${PROJECT_NAME})
endif()
