# BOOST_ROOT
# LibGit2_ROOT or LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARY
# Python3_ROOT_DIR
# SFML_ROOT (lib/cmake/SFML)
# #
# # = Achieve extra staticness =
# # SFML -DBUILD_SHARED_LIBS=OFF -DSFML_USE_STATIC_STD_LIBS=ON
# #set(Boost_USE_STATIC_RUNTIME ON)
# #set(PS_OPTS ${PS_OPTS} "/MT$<$<CONFIG:Debug>:d>")
# # libgit2 -DSTATIC_CRT=OFF
#
# https://stackoverflow.com/questions/9998679/is-it-possible-to-add-post-install-commands-to-top-level-makefile-generated-by-c/29979349#29979349
# https://gitlab.kitware.com/cmake/cmake/issues/14089
#   WORKING_DIRECTORY generator expression support since CMake 3.13
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW) # PackageName_ROOT variables
project(test)

option(PS_DEBUG_WAIT "waits for debugger at startup (win32) (ON/OFF)" OFF)
set(PS_CONFIG_ONLY OFF CACHE BOOL "")
set(PS_CONFIG_SERVER_PYTHON_MODULE_NAME config_server_default CACHE STRING "")
set(PS_CONFIG_UPDATER_PYTHON_MODULE_NAME config_updater_default CACHE STRING "")
set(PS_CONFIG_DEPLOY_PYTHON_MODULE_NAME config_server_default CACHE STRING "")
set(PS_ANSIBLE_PLAYBOOK_CMD ansible-playbook CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

if(WIN32)
	set(SEP "\\;")
else()
	set(SEP ":")
endif()
set(DATASRC ${CMAKE_SOURCE_DIR}/../data/)
set(PYSRC ${CMAKE_SOURCE_DIR}/../files/)
set(PYPATH "PYTHONPATH=${CMAKE_BINARY_DIR}${SEP}${PYSRC}")

configure_file("${PYSRC}${PS_CONFIG_DEPLOY_PYTHON_MODULE_NAME}.py" ps_config_deploy.py @ONLY)
configure_file("${PYSRC}${PS_CONFIG_SERVER_PYTHON_MODULE_NAME}.py" ps_config_server.py @ONLY)
configure_file("${PYSRC}${PS_CONFIG_UPDATER_PYTHON_MODULE_NAME}.py" ps_config_updater.py @ONLY)

add_custom_target(TestsDeployCoor
	COMMAND
		${PS_ANSIBLE_PLAYBOOK_CMD}
			--extra-vars "ps_cmake_binary_dir=${CMAKE_BINARY_DIR}"
			"${PYSRC}/../ansible/deploy.yml"
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(PS_CONFIG_ONLY)
	return()
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem regex)
find_package(LibGit2 REQUIRED)
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML CONFIG COMPONENTS graphics system window REQUIRED)

set(PS_RCS $<$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>:updater.rc>)

add_library(common STATIC cruft.h cruft.cpp miniz/miniz.c ps_config_updater.h)
target_link_libraries(common PUBLIC
	Boost::boost Boost::date_time Boost::filesystem Boost::regex Boost::disable_autolinking
	Threads::Threads LibGit2::LibGit2 $<$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>:winhttp Rpcrt4 crypt32>
)
target_compile_definitions(common PUBLIC _CRT_SECURE_NO_WARNINGS)
target_include_directories(common PUBLIC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/miniz)

add_executable(updater ${PS_RCS} updater.cpp)
target_link_libraries(updater PUBLIC common)
set_target_properties(updater PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<0:>")
# https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html#prop_tgt:RUNTIME_OUTPUT_DIRECTORY
#   RUNTIME_OUTPUT_DIRECTORY "" or "." means CMAKE_BINARY_DIR
#   VS etc helpfully "... append a per-configuration subdirectory to the specified directory unless a generator expression is used"
#     thus landing the binaries into eg CMAKE_BINARY_DIR/Debug
#   use a no-op generator expression to really land binaries into CMAKE_BINARY_DIR

add_executable(stage2 ${PS_RCS} stage2.cpp)
target_link_libraries(stage2 PUBLIC common sfml-graphics sfml-system sfml-window)

add_executable(tupdater2 ${PS_RCS} tupdater.cpp)
target_link_libraries(tupdater2 PUBLIC common)
target_compile_definitions(tupdater2 PUBLIC _PS_DEBUG_TUPDATER=2)

add_executable(tupdater3 ${PS_RCS} tupdater.cpp)
target_link_libraries(tupdater3 PUBLIC common)
target_compile_definitions(tupdater3 PUBLIC _PS_DEBUG_TUPDATER=3)

find_package(Python3 REQUIRED)

add_custom_command(
	COMMENT "Generating Config Header (ps_config_updater.h)"
	OUTPUT ps_config_updater.h
	COMMAND
		${CMAKE_COMMAND} -E env "${PYPATH}"
		${Python3_EXECUTABLE} -m genhdr
			--src_mod ${PS_CONFIG_UPDATER_PYTHON_MODULE_NAME}
			--dst ps_config_updater.h
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	DEPENDS ${PYSRC}${PS_CONFIG_UPDATER_PYTHON_MODULE_NAME}.py ${PYSRC}genhdr.py
)

add_custom_target(TestsServerPreload
	COMMENT "Creating test repository"
	COMMAND
		${CMAKE_COMMAND} -E env "${PYPATH}"
		${Python3_EXECUTABLE} -m preload
			--updater_exe=$<TARGET_FILE:updater>
			--stage2_exe=$<TARGET_FILE:stage2>
			--repo_dir=$<TARGET_FILE_DIR:updater>/repo_s
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	DEPENDS ps_config_server.py
)

add_custom_target(TestsServerRun
	COMMAND
		${CMAKE_COMMAND} -E env "${PYPATH}"
		${Python3_EXECUTABLE} -m startup
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	DEPENDS ps_config_server.py
)

add_custom_target(Tests
	COMMAND
		${CMAKE_COMMAND} -E env "${PYPATH}"
		${Python3_EXECUTABLE} -m pytest ${PYSRC}
			--cov=. --cov-report term
			--customopt_debug_wait=${PS_DEBUG_WAIT}
			--customopt_python_exe=${Python3_EXECUTABLE}
			--customopt_tupdater2_exe=$<TARGET_FILE:tupdater2>
			--customopt_tupdater3_exe=$<TARGET_FILE:tupdater3>
			--customopt_updater_exe=$<TARGET_FILE:updater>
	WORKING_DIRECTORY ${PYSRC}
	DEPENDS ps_config_server.py ps_config_updater.py
)

add_custom_target(TestsDeploy
	COMMAND
		${CMAKE_COMMAND} -E env "${PYPATH}"
		${Python3_EXECUTABLE} -m deploy
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	DEPENDS ps_config_deploy.py
)

install(TARGETS updater stage2 RUNTIME DESTINATION ".")
install(
	FILES
		${CMAKE_BINARY_DIR}/ps_config_deploy.py
		${CMAKE_BINARY_DIR}/ps_config_server.py
		${CMAKE_BINARY_DIR}/ps_config_updater.py
		${PYSRC}/coor.py
		${PYSRC}/server.py
		${PYSRC}/server_coor.py
		${PYSRC}/startup.py
		${PYSRC}/timestamp.py
	DESTINATION "web"
)
install(
	FILES
		${DATASRC}/ps_coor.service
		${DATASRC}/ps_coor_starter.sh
		${DATASRC}/ps_updater.service
		${DATASRC}/ps_updater_starter.sh
	DESTINATION "data"
)
