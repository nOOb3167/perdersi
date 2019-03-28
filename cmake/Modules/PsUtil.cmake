if(WIN32)
	set(SEP "\\;")
else()
	set(SEP ":")
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html#prop_tgt:RUNTIME_OUTPUT_DIRECTORY
#   RUNTIME_OUTPUT_DIRECTORY "" or "." means CMAKE_BINARY_DIR
#   VS etc helpfully "... append a per-configuration subdirectory to the specified directory unless a generator expression is used"
#     thus landing the binaries into eg CMAKE_BINARY_DIR/Debug
#   use a no-op generator expression to really land binaries into CMAKE_BINARY_DIR
set(PS_FORCED_EMPTY_RUNTIME_OUTPUT_DIRECTORY "$<0:>")

function(PS_UTIL_GENERATE_HEADER FNAME HDRNAME)
    add_custom_command(
        COMMENT "Generating Data Header (${HDRNAME})"
        OUTPUT ${HDRNAME}
        COMMAND
            ${CMAKE_COMMAND} -E env "${PYPATH}"
            ${Python3_EXECUTABLE} -m genhdr
                --src_pth ${FNAME}
                --dst ${HDRNAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${FNAME} ${CMAKE_SOURCE_DIR}/files/genhdr.py
    )
endfunction()

function(PS_UTIL_CONVERT_SVG_PNG FNAME PNGNAME)
    add_custom_command(
        COMMENT "Converting SVG to PNG (${FNAME})"
        OUTPUT ${PNGNAME}
        COMMAND
            ${INKSCAPE_EXECUTABLE} --file=${FNAME} --export-png=${PNGNAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${FNAME}
    )
endfunction()

function(PS_UTIL_PCHIZE)
	cmake_parse_arguments(PARSE_ARGV 0 A "" TARGET "PCHBASNAM;CXXSOURCES")
	if(A_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR)
	endif()
	if(MSVC)
		cmake_policy(PUSH)
		cmake_policy(SET CMP0076 NEW) # target_sources relative paths
		target_sources(${A_TARGET} PUBLIC src/${A_PCHBASNAM}.cpp src/${A_PCHBASNAM}.h)
		cmake_policy(POP)
		set_source_files_properties(src/${A_PCHBASNAM}.cpp PROPERTIES COMPILE_FLAGS /Yc${A_PCHBASNAM}.h OBJECT_OUTPUTS ${CMAKE_BINARY_DIR}/${A_PCHBASNAM}.pch)
		set_source_files_properties(${A_CXXSOURCES} PROPERTIES COMPILE_FLAGS /Yu${A_PCHBASNAM}.h OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/${A_PCHBASNAM}.pch)
	endif()
endfunction()
