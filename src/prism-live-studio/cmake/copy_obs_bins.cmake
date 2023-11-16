# CONFIG OBS_SRC_DIR OBS_BUILD_DIR PRISM_SRC_DIR PRISM_BUILD_DIR DEV_OUTPUT_DIR ARCH

set(OBS_BIN_DIR ${OBS_BUILD_DIR}/rundir/${CONFIG})

message(STATUS "Copy obs bins.")
message(STATUS "OBS_SRC_DIR=${OBS_SRC_DIR}")
message(STATUS "OBS_BUILD_DIR=${OBS_BUILD_DIR}")
message(STATUS "PRISM_SRC_DIR=${PRISM_SRC_DIR}")
message(STATUS "PRISM_BUILD_DIR=${PRISM_BUILD_DIR}")
message(STATUS "OBS_BIN_DIR=${OBS_BIN_DIR}")
message(STATUS "DEV_OUTPUT_DIR=${DEV_OUTPUT_DIR}")

if(${CONFIG} STREQUAL "Debug")
    set(OUTPUT_DIR "${DEV_OUTPUT_DIR}/Debug")
else()
    set(OUTPUT_DIR "${DEV_OUTPUT_DIR}/Release")
endif()

file(GLOB files ${OBS_BIN_DIR}/*)
foreach(file ${files})
	message(STATUS "Copy file, from ${file} to ${OUTPUT_DIR}")
	file(COPY "${file}" DESTINATION "${OUTPUT_DIR}" 
		PATTERN "obs64.*" EXCLUDE
		PATTERN "data/obs-studio" EXCLUDE
		PATTERN "data/obs-studio/*" EXCLUDE
		PATTERN "data/obs-plugins/decklink-output-ui" EXCLUDE
		PATTERN "data/obs-plugins/decklink-output-ui/*" EXCLUDE
		PATTERN "data/obs-plugins/frontend-tools" EXCLUDE
		PATTERN "data/obs-plugins/frontend-tools/*" EXCLUDE
		PATTERN "data/obs-plugins/decklink-captions" EXCLUDE
		PATTERN "data/obs-plugins/decklink-captions/*" EXCLUDE
		PATTERN "data/obs-plugins/aja-output-ui" EXCLUDE
		PATTERN "data/obs-plugins/aja-output-ui/*" EXCLUDE
		PATTERN "obs-plugins/64bit/decklink-output-ui.*" EXCLUDE
		PATTERN "obs-plugins/64bit/frontend-tools.*" EXCLUDE
		PATTERN "obs-plugins/64bit/decklink-captions.*" EXCLUDE
		PATTERN "obs-plugins/64bit/aja-output-ui.*" EXCLUDE)
endforeach()
