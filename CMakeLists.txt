cmake_minimum_required(VERSION 3.1)

project(res2h)

# Make sure we generate a compile command database for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-------------------------------------------------------------------------------
#set up compiler flags and excutable names

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	#set up compiler flags for Clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O2") #support C++14 for std::, optimize
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2 -s") #optimize, strip binary
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	#check for G++ 8.0+
	execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE G++_VERSION)
	if (G++_VERSION VERSION_LESS 8.0)
			message(SEND_ERROR "You need at least g++ 8 to compile res2h!")
	endif()
	#set up compiler flags for GCC
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O2") #support C++14 for std::, optimize
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2 -s") #optimize, strip binary
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
	set(CMAKE_DEBUG_POSTFIX "d")
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
	add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") #multi-processor compilation
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP") #multi-processor compilation
endif()

#-------------------------------------------------------------------------------

#define basic sources and headers
set(R2H_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2h.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/checksum.h
)

set(R2H_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2h.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/checksum.cpp
)

#define headers and sources for dump executable
set(R2H_DUMP_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2hinterface.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/checksum.h
)

set(R2H_DUMP_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2hinterface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2hdump.cpp
)

set(R2H_DUMP_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2hinterface.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2hdump.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/checksum.cpp
)

#define headers and sources for unit test executable
set(R2H_UNITTEST_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/res2h.h
)

set(R2H_UNITTEST_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/test.cpp
)

#-------------------------------------------------------------------------------
#define OS specific sources and headers

if(MSVC)
	LIST(APPEND R2H_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/res2h.rc
	)
endif()

#-------------------------------------------------------------------------------
#define libraries and directories

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	set(R2H_LIBRARIES
		stdc++fs
	)
endif()

#-------------------------------------------------------------------------------
#set up build directories

set(dir ${CMAKE_CURRENT_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

#-------------------------------------------------------------------------------
#define target

include_directories(${R2H_INCLUDE_DIRS})
add_executable(res2h ${R2H_SOURCES} ${R2H_HEADERS})
target_link_libraries(res2h ${R2H_LIBRARIES})

#add dump executable to project
add_executable(res2hdump ${R2H_DUMP_SOURCES} ${R2H_DUMP_HEADERS})
target_link_libraries(res2hdump ${R2H_LIBRARIES})

#add unit test executable to project
add_executable(unittest ${R2H_UNITTEST_SOURCES} ${R2H_UNITTEST_HEADERS})
target_link_libraries(unittest ${R2H_LIBRARIES})
