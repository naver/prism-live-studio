

set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)
file(GLOB_RECURSE GuideTips-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE GuideTips-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE GuideTips-UI_FILES *.ui)
file(GLOB_RECURSE GuideTips-RCC_FILES *.qrc)

set(GuideTips-SRC_FILES ${GuideTips-SRC_FILES} PARENT_SCOPE)
set(GuideTips-HDR_FILES ${GuideTips-HDR_FILES} PARENT_SCOPE)
set(GuideTips-UI_FILES ${GuideTips-UI_FILES} PARENT_SCOPE)
set(GuideTips-RCC_FILES ${GuideTips-RCC_FILES} PARENT_SCOPE)
