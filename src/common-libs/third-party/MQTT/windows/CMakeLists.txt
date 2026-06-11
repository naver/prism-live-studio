add_library(common-libs::third-party::mosquitto SHARED IMPORTED GLOBAL)
set_target_properties(common-libs::third-party::mosquitto PROPERTIES
 	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
 	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquitto.lib"
 	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquitto.lib"
 	IMPORTED_IMPLIB_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquitto.lib"
	IMPORTED_IMPLIB_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquitto.lib"
 	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug/mosquitto.dll"
 	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release/mosquitto.dll"
 	IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin/RelWithDebInfo/mosquitto.dll"
	IMPORTED_LOCATION_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin/RelWithDebInfo/mosquitto.dll"
)

add_library(common-libs::third-party::mosquittopp SHARED IMPORTED GLOBAL)
set_target_properties(common-libs::third-party::mosquittopp PROPERTIES
 	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
 	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquittopp.lib"
 	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquittopp.lib"
 	IMPORTED_IMPLIB_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquittopp.lib"
	IMPORTED_IMPLIB_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/lib/mosquittopp.lib"
 	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug/mosquittopp.dll"
 	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release/mosquittopp.dll"
 	IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin/RelWithDebInfo/mosquittopp.dll"
	IMPORTED_LOCATION_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin/RelWithDebInfo/mosquittopp.dll"
)

add_library(common-libs::third-party::mqtt INTERFACE IMPORTED GLOBAL)
set_property(TARGET common-libs::third-party::mqtt PROPERTY
   INTERFACE_LINK_LIBRARIES common-libs::third-party::mosquitto common-libs::third-party::mosquittopp
)

set_target_properties(common-libs::third-party::mqtt PROPERTIES FOLDER common-libs/third-party)

function(copy_third_party_mqtt target)
	copy_target(${target} "mosquitto" "${COMMON_LIBS_DIR}/third-party/mqtt/windows/bin" bin/64bit)
	copy_target(${target} "mosquittopp" "${COMMON_LIBS_DIR}/third-party/mqtt/windows/bin" bin/64bit)
endfunction()



