project(PLSAppLoadingView)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(UI_FILES
    PLSAppLoadingView.ui
)
set(SOURCE_FILES
    PLSAppLoadingView.cpp
    PLSAppLoadingView.h
    main.cpp
)
set(RESOURCE_FILES
    resources.qrc
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES} ${RESOURCE_FILES} ${UI_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER common-libs)
target_link_libraries(${PROJECT_NAME} Qt6::Core Qt6::Widgets Qt6::Gui common-libs::ui common-libs::utils-api common-libs::log common-libs::dump-analyzer)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR} 
)
common_libs_exe(${PROJECT_NAME} "PLSAppLoadingView")
if(OS_WINDOWS)
copy_common_libs_binary(${PROJECT_NAME})
elseif(OS_MACOS)
copy_excutable_target(${PROJECT_NAME})
endif()
