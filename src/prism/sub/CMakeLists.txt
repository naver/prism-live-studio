cmake_minimum_required(VERSION 3.1.0)

file(GLOB pros *)
foreach(pro ${pros})
    if(IS_DIRECTORY ${pro})
	    if(EXISTS ${pro}/CMakeLists.txt)
	        add_subdirectory(${pro})
		endif()	
    endif()
endforeach()
    

