add_library(common-libs::third-party::openssl SHARED IMPORTED GLOBAL)
set_target_properties(
	common-libs::third-party::openssl PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(
	common-libs::third-party::openssl PROPERTIES
	IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/libcrypto.lib;${CMAKE_CURRENT_SOURCE_DIR}/lib/libssl.lib"
	IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/libcrypto.lib;${CMAKE_CURRENT_SOURCE_DIR}/lib/libssl.lib"
	IMPORTED_IMPLIB_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/lib/libcrypto.lib;${CMAKE_CURRENT_SOURCE_DIR}/lib/libssl.lib"
	IMPORTED_IMPLIB_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/lib/libcrypto.lib;${CMAKE_CURRENT_SOURCE_DIR}/lib/libssl.lib"
	IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/libcrypto-1_1-x64.dll;${CMAKE_CURRENT_SOURCE_DIR}/bin/libssl-1_1-x64.dll"
	IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/libcrypto-1_1-x64.dll;${CMAKE_CURRENT_SOURCE_DIR}/bin/libssl-1_1-x64.dll"
	IMPORTED_LOCATION_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin/libcrypto-1_1-x64.dll;${CMAKE_CURRENT_SOURCE_DIR}/bin/libssl-1_1-x64.dll"
	IMPORTED_LOCATION_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin/libcrypto-1_1-x64.dll;${CMAKE_CURRENT_SOURCE_DIR}/bin/libssl-1_1-x64.dll")

function(copy_third_party_openssl target)
  if(OS_WINDOWS)
	copy_target(${target} "libcrypto-1_1-x64" "${COMMON_LIBS_DIR}/third-party/openssl/windows/bin" bin/64bit)
	copy_target(${target} "libssl-1_1-x64" "${COMMON_LIBS_DIR}/third-party/openssl/windows/bin" bin/64bit)
  elseif(OS_MACOS)
  endif()
endfunction()