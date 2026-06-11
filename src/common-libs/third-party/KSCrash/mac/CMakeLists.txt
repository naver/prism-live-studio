
add_library(common-libs::third-party::kscrash SHARED IMPORTED GLOBAL)
set_target_properties(
    common-libs::third-party::kscrash PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/KSCrash.framework/Versions/Current/Headers")
set_target_properties(
    common-libs::third-party::kscrash PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/KSCrash.framework/Versions/Current/KSCrash"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/KSCrash.framework/Versions/Current/KSCrash"
)

copy_mac_framework_data(KSCrash.framework)
