

set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)
file(GLOB_RECURSE PLSResourcesMgr-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE PLSResourcesMgr-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE PLSResourcesMgr-UI_FILES *.ui)

set(PLSResourcesMgr-SRC_FILES ${PLSResourcesMgr-SRC_FILES} PARENT_SCOPE)
set(PLSResourcesMgr-HDR_FILES ${PLSResourcesMgr-HDR_FILES} PARENT_SCOPE)
set(PLSResourcesMgr-UI_FILES ${PLSResourcesMgr-UI_FILES} PARENT_SCOPE)

