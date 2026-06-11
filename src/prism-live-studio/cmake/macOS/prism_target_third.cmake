include_guard(GLOBAL)

function(prism_target_link_hmac target)
	find_library(APIHMAC ApiGatewayHMAC HINTS "$ENV{SRC_DIR}/common-libs/third-party/hmac/mac/${CMAKE_OSX_ARCHITECTURES}")
	target_link_libraries(${target} "$<LINK_LIBRARY:FRAMEWORK,${APIHMAC}>")
endfunction()

function(prism_target_link_nelo target)
	find_library(NELOSDK NeloSDK HINTS "$ENV{SRC_DIR}/common-libs/third-party/nelo/mac")
	target_link_libraries(${target} "$<LINK_LIBRARY:FRAMEWORK,${NELOSDK}>")
endfunction()

function(prism_target_link_kscrash target)
	find_library(KSCRASH KSCrash HINTS "$ENV{SRC_DIR}/common-libs/third-party/KSCrash/mac")
	target_link_libraries(${target} "$<LINK_LIBRARY:FRAMEWORK,${KSCRASH}>")
endfunction()