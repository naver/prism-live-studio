file(GLOB_RECURSE test-tools-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE test-tools-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE test-tools-UI_FILES *.ui)

set(test-tools-SRC_FILES ${test-tools-SRC_FILES} PARENT_SCOPE)
set(test-tools-HDR_FILES ${test-tools-HDR_FILES} PARENT_SCOPE)
set(test-tools-UI_FILES ${test-tools-UI_FILES} PARENT_SCOPE)