# BOOST_ROOT
# LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARIES
# Python3_ROOT_DIR
cmake_minimum_required(VERSION 3.0)
project(test)

option(PS_WAIT_DEBUG "waits for debugger at startup (win32) (ON/OFF)" OFF)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake/Modules
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem regex)
find_package(LibGit2 REQUIRED)

SET(PS_INCS PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/miniz ${LIBGIT2_INCLUDE_DIR})
SET(PS_LIBS Boost::boost Boost::date_time Boost::filesystem Boost::regex Boost::disable_autolinking Threads::Threads ${LIBGIT2_LIBRARIES} ${SHLWAPI_LIBRARIES})
if (WIN32)
	SET(PS_LIBS ${PS_LIBS} winhttp Rpcrt4 crypt32)
	SET(PS_OPTS PUBLIC "/MT$<$<CONFIG:Debug>:d>")
endif ()
if (PS_WAIT_DEBUG)
	SET(PS_DEFS ${PS_DEFS} _PS_DEBUG_WAIT)
endif ()

add_executable(test updater.cpp miniz/miniz.c)
target_compile_options(test ${PS_OPTS})
target_compile_definitions(test PUBLIC ${PS_DEFS} _PS_UPDATER_TESTING _CRT_SECURE_NO_WARNINGS)
target_include_directories(test ${PS_INCS})
target_link_libraries(test ${PS_LIBS})

find_package(Python3 REQUIRED)

add_custom_target(Tests
	COMMAND ${Python3_EXECUTABLE}
		-m pytest --cov=. --cov-report term
		--customopt_debug_wait=${PS_WAIT_DEBUG}
		--customopt_python_exe=${Python3_EXECUTABLE}
		--customopt_updater_exe=$<TARGET_FILE:test>
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../files
	COMMENT "Running Tests"
)
add_dependencies(Tests test)

install(TARGETS test RUNTIME DESTINATION bin)
