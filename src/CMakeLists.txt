# BOOST_ROOT
# LIBGIT2_INCLUDE_DIR LIBGIT2_LIBRARIES
# Python3_ROOT_DIR
cmake_minimum_required(VERSION 3.0)
project(test)
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake/Modules
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem regex)
find_package(LibGit2 REQUIRED)
add_executable(foo test.cpp miniz/miniz.c)
target_link_libraries(foo Boost::boost Boost::date_time Boost::filesystem Boost::regex Boost::disable_autolinking)
target_link_libraries(foo Threads::Threads ${LIBGIT2_LIBRARIES} ${SHLWAPI_LIBRARIES})
if (WIN32)
	target_link_libraries(foo winhttp Rpcrt4.lib crypt32.lib)
endif ()
target_compile_options(foo
	PUBLIC "/MT$<$<CONFIG:Debug>:d>"
)
target_include_directories(foo PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/miniz ${LIBGIT2_INCLUDE_DIR})
install(TARGETS foo RUNTIME DESTINATION bin)

find_package(Python3 REQUIRED)

add_custom_target(Tests
	COMMAND ${Python3_EXECUTABLE}
		-m pytest --cov=. --cov-report term
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../
	COMMENT "Running Tests"
)
