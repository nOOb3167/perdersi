# BOOST_ROOT
# LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARIES
# Python3_ROOT_DIR
# SFML_ROOT (lib/cmake/SFML)
# #
# # = Achieve extra staticness =
# # SFML -DBUILD_SHARED_LIBS=OFF -DSFML_USE_STATIC_STD_LIBS=ON
# #set(Boost_USE_STATIC_RUNTIME ON)
# #set(PS_OPTS ${PS_OPTS} "/MT$<$<CONFIG:Debug>:d>")
# # libgit2 -DSTATIC_CRT=OFF
cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0074 NEW) # PackageName_ROOT variables
project(test)

option(PS_DEBUG_WAIT "waits for debugger at startup (win32) (ON/OFF)" OFF)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake/Modules
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem regex)
find_package(LibGit2 REQUIRED)
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML CONFIG COMPONENTS graphics system window REQUIRED)

if (WIN32)
	set(PS_RCS ${PS_RCS} updater.rc)
	set(PS_LIBS ${PS_LIBS} winhttp Rpcrt4 crypt32)
endif ()
set(PS_INCS ${PS_INCS} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/miniz ${LIBGIT2_INCLUDE_DIR})
set(PS_LIBS ${PS_LIBS} Boost::boost Boost::date_time Boost::filesystem Boost::regex Boost::disable_autolinking Threads::Threads ${LIBGIT2_LIBRARIES} ${SHLWAPI_LIBRARIES})
set(PS_DEFS ${PS_DEFS} _PS_UPDATER_TESTING _CRT_SECURE_NO_WARNINGS)

add_library(common STATIC cruft.h cruft.cpp miniz/miniz.c)
target_link_libraries(common PUBLIC ${PS_LIBS})
target_compile_definitions(common PUBLIC ${PS_DEFS})
target_compile_options(common PUBLIC ${PS_OPTS})
target_include_directories(common PUBLIC ${PS_INCS})

add_executable(test ${PS_RCS} updater.cpp)
target_link_libraries(test PUBLIC common)

add_executable(stage2 ${PS_RCS} stage2.cpp)
target_link_libraries(stage2 PUBLIC common sfml-graphics sfml-system sfml-window)

add_executable(tupdater2 ${PS_RCS} tupdater.cpp)
target_link_libraries(tupdater2 PUBLIC common)
target_compile_definitions(tupdater2 PUBLIC _PS_DEBUG_TUPDATER=2)

add_executable(tupdater3 ${PS_RCS} tupdater.cpp)
target_link_libraries(tupdater3 PUBLIC common)
target_compile_definitions(tupdater3 PUBLIC _PS_DEBUG_TUPDATER=3)

find_package(Python3 REQUIRED)

add_custom_target(Tests
	COMMAND ${Python3_EXECUTABLE}
		-m pytest --cov=. --cov-report term
		--customopt_debug_wait=${PS_DEBUG_WAIT}
		--customopt_python_exe=${Python3_EXECUTABLE}
		--customopt_tupdater2_exe=$<TARGET_FILE:tupdater2>
		--customopt_tupdater3_exe=$<TARGET_FILE:tupdater3>
		--customopt_updater_exe=$<TARGET_FILE:test>
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../files
	COMMENT "Running Tests"
)
add_dependencies(Tests test tupdater2 tupdater3)

install(TARGETS test RUNTIME DESTINATION bin)
