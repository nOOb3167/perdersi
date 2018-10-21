cmake_minimum_required(VERSION 3.0)
project(test)
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake/Modules
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost 1.66 REQUIRED COMPONENTS date_time filesystem regex)
find_package(LibGit2 REQUIRED)
add_executable(foo test.cpp)
target_link_libraries(foo Boost::boost Boost::date_time Boost::filesystem Boost::regex)
target_link_libraries(foo Threads::Threads ${LIBGIT2_LIBRARIES} ${SHLWAPI_LIBRARIES})
target_include_directories(foo PUBLIC ${CMAKE_SOURCE_DIR}/include ${LIBGIT2_INCLUDE_DIR})
install(TARGETS foo RUNTIME DESTINATION bin)
