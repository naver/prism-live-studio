project(EnvCheck)

message(STATUS "Sub: EnvCheck")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_executable(EnvCheck EnvCheck.cpp)

target_link_libraries(EnvCheck
	obs
	)

add_obs_dependency(EnvCheck)
link_obs(EnvCheck)

install_obs_core(EnvCheck)
copy_prism_target(EnvCheck)

