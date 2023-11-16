
project(PRISMRelaunch)

find_library(APPKIT Appkit)

add_executable(PRISMRelaunch relaunch.mm)

set_source_files_properties(relaunch.mm PROPERTIES COMPILE_FLAGS -fobjc-arc)

set_target_properties(PRISMRelaunch PROPERTIES FOLDER common-libs/PRISMRelaunch)

target_link_libraries(PRISMRelaunch PRIVATE ${APPKIT})

target_compile_options(PRISMRelaunch PRIVATE -fobjc-arc)

copy_prism_binary(PRISMRelaunch)



