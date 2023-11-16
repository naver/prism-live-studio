

add_library(common-libs::third-party::mosquitto SHARED IMPORTED GLOBAL)
add_library(common-libs::third-party::mosquittopp SHARED IMPORTED GLOBAL)

set_target_properties(common-libs::third-party::mosquitto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/universal/include"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/universal/lib/libmosquitto.dylib"
    IMPORTED_SONAME "@rpath/libmosquitto.1.dylib")

set_target_properties(common-libs::third-party::mosquittopp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/universal/include"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/universal/lib/libmosquittopp.dylib"
    IMPORTED_SONAME "@rpath/libmosquittopp.1.dylib")
copy_mac_dylibs_data(universal/lib)

add_library(common-libs::third-party::mqtt INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::mqtt PROPERTY
    INTERFACE_LINK_LIBRARIES common-libs::third-party::mosquitto common-libs::third-party::mosquittopp
)

set_target_properties(common-libs::third-party::mqtt PROPERTIES FOLDER common-libs/third-party)
set_extra_dylib_path("${CMAKE_CURRENT_SOURCE_DIR}/universal/lib")