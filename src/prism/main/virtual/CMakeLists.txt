

set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)
file(GLOB_RECURSE PLSVirtualBg-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE PLSVirtualBg-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE PLSVirtualBg-UI_FILES *.ui)

set(PLSVirtualBg-SRC_FILES ${PLSVirtualBg-SRC_FILES} PARENT_SCOPE)
set(PLSVirtualBg-HDR_FILES ${PLSVirtualBg-HDR_FILES} PARENT_SCOPE)
set(PLSVirtualBg-UI_FILES ${PLSVirtualBg-UI_FILES} PARENT_SCOPE)

