file(GLOB dirs *-tests)

foreach(dir ${dirs})
	if(IS_DIRECTORY ${dir})
		add_subdirectory(${dir})
	endif()
endforeach()