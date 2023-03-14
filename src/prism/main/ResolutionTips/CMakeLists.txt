

set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)
file(GLOB_RECURSE PLSResolution-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE PLSResolution-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE PLSResolution-UI_FILES *.ui)
file(GLOB_RECURSE PLSResolution-RCC_FILES *.qrc)

set(PLSResolution-SRC_FILES ${PLSResolution-SRC_FILES} PARENT_SCOPE)
set(PLSResolution-HDR_FILES ${PLSResolution-HDR_FILES} PARENT_SCOPE)
set(PLSResolution-UI_FILES ${PLSResolution-UI_FILES} PARENT_SCOPE)
set(PLSResolution-RCC_FILES ${PLSResolution-RCC_FILES} PARENT_SCOPE)
