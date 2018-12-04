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
