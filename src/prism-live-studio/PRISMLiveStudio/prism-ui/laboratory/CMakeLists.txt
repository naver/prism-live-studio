
set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)

file(GLOB_RECURSE laboratory-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE laboratory-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE laboratory-UI_FILES *.ui)

set(laboratory-SRC_FILES ${laboratory-SRC_FILES} PARENT_SCOPE)
set(laboratory-HDR_FILES ${laboratory-HDR_FILES} PARENT_SCOPE)
set(laboratory-UI_FILES ${laboratory-UI_FILES} PARENT_SCOPE)

