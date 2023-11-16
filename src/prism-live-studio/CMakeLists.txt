list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake/Modules")

include(OBSLibs)

if(OS_MACOS)
list(APPEND CMAKE_MODULE_PATH "$ENV{PRISM_SRC_DIR}/cmake/macOS")
include(PrismDefaults_macOS)
endif()


add_definitions(-DHAVE_OBSCONFIG_H -DHAVE_PLSCONFIG_H -DUNICODE -D_UNICODE)

configure_file("$ENV{PRISM_SRC_DIR}/cmake/prism-version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/prism-version.h")
include_directories(SYSTEM "${CMAKE_CURRENT_BINARY_DIR}")

set(OBS_ENV_PROJECT_DIR $ENV{PROJECT_DIR})
set(OBS_ENV_CEF_ROOT_DIR $ENV{CEF_ROOT_DIR})
set(OBS_ENV_VLC_DIR $ENV{VLC_DIR})
set(OBS_ENV_VIRTUALCAM_GUID $ENV{VIRTUALCAM_GUID})
set(OBS_ENV_ALL_DEPS $ENV{ALL_DEPS})
set(OBS_ENV_GENERATOR $ENV{GENERATOR})
set(OBS_ENV_COMPILER $ENV{COMPILER})
set(OBS_ENV_ARCH $ENV{ARCH})
set(OBS_ENV_SUFFIX $ENV{SUFFIX})
set(OBS_ENV_VERSION $ENV{VERSION})
set(OBS_ENV_BUILD_TYPE $ENV{BUILD_TYPE})
set(OBS_ENV_PRISM_VERSION_H_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(OBS_ENV_OSX_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
set(OBS_ENV_SWIG_DIR ${SWIG_DIR})

if(OS_WINDOWS)

	file(GENERATE
		 OUTPUT "${CMAKE_BINARY_DIR}/ALL_BUILD.vcxproj.user"
	     INPUT "$ENV{PRISM_SRC_DIR}/cmake/bundle/windows/ALL_BUILD.vcxproj.user.in")

	configure_file("$ENV{PRISM_SRC_DIR}/cmake/obs-env.cmd.in" "${CMAKE_CURRENT_BINARY_DIR}/obs-env.cmd")
	execute_process(COMMAND $ENV{ComSpec} /c $ENV{PRISM_SRC_DIR}/cmake/obs-configure ${CMAKE_CURRENT_BINARY_DIR}/obs-env.cmd)

elseif(OS_MACOS)

	configure_file("$ENV{PRISM_SRC_DIR}/cmake/macOS/obs-env.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/obs-env.sh" @ONLY)
	# generator obs xcode project
	execute_process(COMMAND sh obs_configure.sh ${CMAKE_CURRENT_BINARY_DIR}/obs-env.sh WORKING_DIRECTORY $ENV{PRISM_SRC_DIR}/cmake/macOS)

endif()

add_executable(obs-build cmake/build.cpp)
set_target_properties(obs-build PROPERTIES FOLDER "prism/obs")

if(OS_WINDOWS)
add_custom_command(TARGET obs-build
	PRE_BUILD
	COMMAND $ENV{ComSpec} /c
		"$ENV{PRISM_SRC_DIR}/cmake/obs-build"
		"${CMAKE_CURRENT_BINARY_DIR}/obs-env.cmd"
		"$<CONFIGURATION>")
elseif(OS_MACOS)

add_custom_command(TARGET obs-build
	PRE_BUILD
	COMMAND sh
		"$ENV{PRISM_SRC_DIR}/cmake/macOS/obs-build.sh"
		"${CMAKE_CURRENT_BINARY_DIR}/obs-env.sh"
		"$ENV{BUILD_TYPE}" WORKING_DIRECTORY $ENV{PRISM_SRC_DIR}/cmake/macOS)
endif()

if(OS_WINDOWS)
add_dependencies(obs-build libutils-api liblog libhttp-client libipc libtask libui libdump-analyzer)
endif()

add_executable(obs-copy cmake/build.cpp)
set_target_properties(obs-copy PROPERTIES FOLDER "prism/obs")
if(OS_WINDOWS)
add_custom_command(TARGET obs-copy
	POST_BUILD
	COMMAND "${CMAKE_COMMAND}"
		"-DCONFIG=$<CONFIGURATION>"
		"-DOBS_SRC_DIR=$ENV{OBS_SRC_DIR}"
		"-DOBS_BUILD_DIR=$ENV{OBS_BUILD_DIR}"
		"-DPRISM_SRC_DIR=$ENV{PRISM_SRC_DIR}"
		"-DPRISM_BUILD_DIR=$ENV{PRISM_BUILD_DIR}"
		"-DDEV_OUTPUT_DIR=$ENV{DEV_OUTPUT_DIR}"
		"-DARCH=$ENV{ARCH}"
		-P "$ENV{PRISM_SRC_DIR}/cmake/copy_obs_bins.cmake"
	VERBATIM)
add_dependencies(obs-copy obs-build)
endif()

add_subdirectory(common)
if(OS_WINDOWS)
add_subdirectory(PRISMLiveStudio)
elseif(OS_MACOS)
add_subdirectory(PRISMLiveStudio)
endif()
