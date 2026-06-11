
add_library(common-libs::third-party::libcrypto SHARED IMPORTED GLOBAL)
add_library(common-libs::third-party::libssl SHARED IMPORTED GLOBAL)

set_target_properties(common-libs::third-party::libcrypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/universal/include"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/universal/lib/libcrypto.dylib"
    IMPORTED_SONAME "@rpath/libcrypto.1.1.dylib")

set_target_properties(common-libs::third-party::libssl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/universal/include"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/universal/lib/libssl.dylib"
    IMPORTED_SONAME "@rpath/libssl.1.1.dylib")
copy_mac_dylibs_data(universal/lib)

add_library(common-libs::third-party::openssl INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::openssl PROPERTY
    INTERFACE_LINK_LIBRARIES common-libs::third-party::libcrypto common-libs::third-party::libssl
)

set_target_properties(common-libs::third-party::openssl PROPERTIES FOLDER common-libs/third-party)

set_extra_dylib_path("${CMAKE_CURRENT_SOURCE_DIR}/universal/lib")