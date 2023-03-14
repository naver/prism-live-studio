project(PRISMSplashScreen)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${Qt5Widgets_PRIVATE_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

configure_file(${PRISMLiveStudio_SOURCE_DIR}/pls.rc.in "${CMAKE_CURRENT_BINARY_DIR}/pls.rc")

qt5_wrap_ui(SPLASH_SCREEN_UI_HEADERS PLSSplashScreen.ui)
qt5_add_resources(SPLASH_SCREEN_QRC_SOURCES SplashScreen.qrc)

add_executable(PRISMSplashScreen WIN32
	main.cpp
	PLSSplashScreen.cpp
	PLSSplashScreen.hpp
	${SPLASH_SCREEN_UI_HEADERS}
	${SPLASH_SCREEN_QRC_SOURCES}
	"${CMAKE_CURRENT_BINARY_DIR}/pls.rc")
target_link_libraries(PRISMSplashScreen
	Qt5::Widgets
	frontend-api)
add_dependencies(PRISMSplashScreen
    frontend-api
    PRISMLiveStudio)

install_obs_core(PRISMSplashScreen)
copy_prism_target(PRISMSplashScreen)
