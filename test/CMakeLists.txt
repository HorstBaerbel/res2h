cmake_minimum_required(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

#-------------------------------------------------------------------------------
# Set up compiler flags and excutable names
include(compiler_checks)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	# set up compiler flags for Clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14") # support C++14
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	# check for g++ 7.0+
	execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE G++_VERSION)
	if (G++_VERSION VERSION_LESS 7.0)
			message(SEND_ERROR "You need at least g++ 7 to compile res2h!")
	endif()
	# set up compiler flags for GCC
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14") # support C++14
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
	set(CMAKE_DEBUG_POSTFIX "d")
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
	add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # multi-processor compilation
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP") # multi-processor compilation
endif()

# Add some compiler flags so we see warnings early on
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wall")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wextra")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Weffc++")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wconversion")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wnon-virtual-dtor")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wold-style-cast")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Woverloaded-virtual")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wself-init")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wunsafe-loop-optimization")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-pedantic")

# Enable easier debugging for unit tests
#EnableIfCompilerSupports( CMAKE_CXX_FLAGS "-Og" )
EnableIfCompilerSupports( CMAKE_CXX_FLAGS "-g" )

#-------------------------------------------------------------------------------
# Macro to add tests with all files and definitions involved
macro(AddTest name)
	add_executable(test_${name} test_${name}.cpp)
	target_link_libraries(test_${name} ${TEST_LIBRARIES})
	add_test(${name} test_${name})
endmacro()

#-------------------------------------------------------------------------------
# Add unit tests here
set(CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(PROJECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
include_directories(${CMAKE_SOURCE_DIR})

set(TEST_LIBRARIES
	r2hlib
	stdc++fs
)

AddTest(checksum)
AddTest(fshelpers)
