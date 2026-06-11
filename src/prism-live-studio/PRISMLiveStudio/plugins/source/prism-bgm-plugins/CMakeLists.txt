project(prism-bgm-plugins)

add_library(prism-bgm-plugins MODULE)
add_library(OBS::prism-bgm-plugins ALIAS prism-bgm-plugins)
add_definitions(-D_PRISM_PLUGIN)

target_sources(prism-bgm-plugins PRIVATE prism-bgm-plugin.cpp prism-bgm-source.cpp)

target_link_libraries(prism-bgm-plugins PRIVATE OBS::libobs common-libs::utils-api common-libs::log prism::frontend-api
    common::log)

if(OS_WINDOWS)
    if(MSVC)
    target_link_libraries(prism-bgm-plugins PRIVATE OBS::w32-pthreads)
  endif()
endif()

prism_module(prism-bgm-plugins "PRISM bgm module")
set_target_properties(prism-bgm-plugins PROPERTIES FOLDER "prism/plugins/source")

copy_prism_plugin_binary(prism-bgm-plugins)

