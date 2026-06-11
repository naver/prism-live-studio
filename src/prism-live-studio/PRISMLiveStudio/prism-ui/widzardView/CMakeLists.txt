

set(CMAKE_AUTOMOC True)
set(CMAKE_AUTOUIC True)
file(GLOB_RECURSE WizardView-SRC_FILES *.c *.cpp *.cxx *.cc)
file(GLOB_RECURSE WizardView-HDR_FILES *.h *.hpp *.hxx *.hh)
file(GLOB_RECURSE WizardView-UI_FILES *.ui)
file(GLOB_RECURSE WizardView-RCC_FILES *.qrc)

set(WizardView-SRC_FILES ${WizardView-SRC_FILES} PARENT_SCOPE)
set(WizardView-HDR_FILES ${WizardView-HDR_FILES} PARENT_SCOPE)
set(WizardView-UI_FILES ${WizardView-UI_FILES} PARENT_SCOPE)
set(WizardView-RCC_FILES ${WizardView-RCC_FILES} PARENT_SCOPE)
