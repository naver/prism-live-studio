include_guard(GLOBAL)

function(prism_target_link_kscrash target)
	find_library(KSCRASH KSCrash HINTS "$ENV{SRC_DIR}/common-libs/third-party/KSCrash/mac")
	target_link_libraries(${target} "$<LINK_LIBRARY:FRAMEWORK,${KSCRASH}>")
endfunction()