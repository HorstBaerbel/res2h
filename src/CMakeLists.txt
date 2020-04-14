cmake_minimum_required(VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(res2h)

# Make sure we generate a compile command database for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# See more suggestions here: https://stackoverflow.com/a/9862800/1121150
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wall")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wextra")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wconversion")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wnon-virtual-dtor")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wold-style-cast")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Woverloaded-virtual")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wself-init")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-Wunsafe-loop-optimization")
EnableIfCompilerSupports(CMAKE_CXX_FLAGS "-pedantic")

#-------------------------------------------------------------------------------
include_directories(${CMAKE_SOURCE_DIR})

# define basic sources and headers
set(R2H_HEADERS
	${PROJECT_SOURCE_DIR}/res2h.h
	${PROJECT_SOURCE_DIR}/checksum.h
)

set(R2H_SOURCES
	${PROJECT_SOURCE_DIR}/res2h.cpp
	${PROJECT_SOURCE_DIR}/checksum.cpp
	${PROJECT_SOURCE_DIR}/stdfshelpers.cpp
	${PROJECT_SOURCE_DIR}/res2hhelpers.cpp
	${PROJECT_SOURCE_DIR}/syshelpers.cpp
)

# define headers and sources for dump executable
set(R2H_DUMP_HEADERS
	${PROJECT_SOURCE_DIR}/res2hinterface.h
	${PROJECT_SOURCE_DIR}/checksum.h
)

set(R2H_DUMP_SOURCES
	${PROJECT_SOURCE_DIR}/res2hinterface.cpp
	${PROJECT_SOURCE_DIR}/res2hdump.cpp
	${PROJECT_SOURCE_DIR}/checksum.cpp
	${PROJECT_SOURCE_DIR}/stdfshelpers.cpp
	${PROJECT_SOURCE_DIR}/res2hhelpers.cpp
	${PROJECT_SOURCE_DIR}/syshelpers.cpp
)

#-------------------------------------------------------------------------------
# define OS specific sources and headers

if(MSVC)
	LIST(APPEND R2H_SOURCES
		${PROJECT_SOURCE_DIR}/res2h.rc
	)
endif()

#-------------------------------------------------------------------------------
# define libraries and directories

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	set(R2H_LIBRARIES
		stdc++fs
	)
endif()

#-------------------------------------------------------------------------------
# Define targets and unit tests

# Make sure we build in release mode by default
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

add_library (r2hlib ${R2H_SOURCES} ${R2H_DUMP_SOURCES})

# Add res2h executable to project
include_directories(${R2H_INCLUDE_DIRS})
add_executable(res2h ${R2H_SOURCES} ${R2H_HEADERS})
target_link_libraries(res2h ${R2H_LIBRARIES})

# Add resh2dump executable to project
add_executable(res2hdump ${R2H_DUMP_SOURCES} ${R2H_DUMP_HEADERS})
target_link_libraries(res2hdump ${R2H_LIBRARIES})
