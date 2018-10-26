# BOOST_ROOT
# LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARIES
# Python3_ROOT_DIR
# SFML_ROOT (lib/cmake/SFML)
cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0074 NEW) # PackageName_ROOT variables
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
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML CONFIG COMPONENTS graphics system window REQUIRED)

set(PS_SRCS ${PS_SRCS} updater.cpp miniz/miniz.c)
set(PS_INCS ${PS_INCS} ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/miniz ${LIBGIT2_INCLUDE_DIR})
set(PS_LIBS ${PS_LIBS} Boost::boost Boost::date_time Boost::filesystem Boost::regex Boost::disable_autolinking sfml-graphics sfml-system sfml-window Threads::Threads ${LIBGIT2_LIBRARIES} ${SHLWAPI_LIBRARIES})
if (WIN32)
	set(PS_LIBS ${PS_LIBS} winhttp Rpcrt4 crypt32)
	set(PS_OPTS ${PS_OPTS} "/MT$<$<CONFIG:Debug>:d>")
endif ()
set(PS_DEFS ${PS_DEFS} _PS_UPDATER_TESTING _CRT_SECURE_NO_WARNINGS)
if (PS_WAIT_DEBUG)
	set(PS_DEFS ${PS_DEFS} _PS_DEBUG_WAIT)
endif ()

add_executable(test ${PS_SRCS})
target_compile_options(test PUBLIC ${PS_OPTS})
target_compile_definitions(test PUBLIC ${PS_DEFS})
target_include_directories(test PUBLIC ${PS_INCS})
target_link_libraries(test ${PS_LIBS})

add_executable(tupdater2 tupdater.cpp)
target_compile_options(tupdater2 PUBLIC ${PS_OPTS})
target_compile_definitions(tupdater2 PUBLIC ${PS_DEFS} _PS_DEBUG_TUPDATER=2)
target_include_directories(tupdater2 PUBLIC ${PS_INCS})
target_link_libraries(tupdater2 ${PS_LIBS})

add_executable(tupdater3 tupdater.cpp)
target_compile_options(tupdater3 PUBLIC ${PS_OPTS})
target_compile_definitions(tupdater3 PUBLIC ${PS_DEFS} _PS_DEBUG_TUPDATER=3)
target_include_directories(tupdater3 PUBLIC ${PS_INCS})
target_link_libraries(tupdater3 ${PS_LIBS})

find_package(Python3 REQUIRED)

add_custom_target(Tests
	COMMAND ${Python3_EXECUTABLE}
		-m pytest --cov=. --cov-report term
		--customopt_debug_wait=${PS_WAIT_DEBUG}
		--customopt_python_exe=${Python3_EXECUTABLE}
		--customopt_tupdater2_exe=$<TARGET_FILE:tupdater2>
		--customopt_tupdater3_exe=$<TARGET_FILE:tupdater3>
		--customopt_updater_exe=$<TARGET_FILE:test>
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../files
	COMMENT "Running Tests"
)
add_dependencies(Tests test tupdater2 tupdater3)

install(TARGETS test RUNTIME DESTINATION bin)
