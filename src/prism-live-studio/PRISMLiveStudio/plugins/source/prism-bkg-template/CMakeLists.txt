project(prism-bkg-template)

add_library(prism-bkg-template MODULE)
add_library(OBS::prism-bkg-template ALIAS prism-bkg-template)
add_definitions(-D_PRISM_PLUGIN)

target_sources(prism-bkg-template PRIVATE plugin-main.cpp 
    prism-bkg-template.cpp
    prism-bkg-template.hpp)

target_link_libraries(prism-bkg-template PRIVATE OBS::libobs common-libs::utils-api common-libs::log prism::frontend-api
    common::log)

if(OS_WINDOWS)
    if(MSVC)
    target_link_libraries(prism-bkg-template PRIVATE OBS::w32-pthreads)
  endif()
endif()

prism_module(prism-bkg-template "PRISM background template module")

set_target_properties(prism-bkg-template PROPERTIES FOLDER "prism/plugins/source")

copy_prism_plugin_binary(prism-bkg-template)

