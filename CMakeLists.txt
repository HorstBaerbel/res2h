cmake_minimum_required(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(res2h)

# Add the reuglar source directory
add_subdirectory(src)

# Add unit tests via CTest
enable_testing ()
add_subdirectory (test)