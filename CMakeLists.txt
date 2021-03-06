#Version of cmake
cmake_minimum_required(VERSION 3.0)

#Name of the Project
project(Server_Vector_Field_Visualization)

#Set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(VRPN_INCLUDE_DIR "" CACHE PATH "Path to the VRPN include directory")
set(VRPN_LIB_DIR     "" CACHE PATH "Path to the VRPN lib directory")
set(QUAT_LIB_DIR     "" CACHE PATH "Path to the QUAT lib directory used by VRPN")
set(RELEASE               FALSE                                   CACHE BOOL "Compiling in release mode.")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(RELEASE)
	MESSAGE(STATUS "Compiling in release mode")
	set(CMAKE_BUILD_TYPE "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++17")
else()
	MESSAGE(STATUS "Compiling in Debug mode")
	set(CMAKE_BUILD_TYPE "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -std=c++17")
endif()

SET(WARNING_FLAGS "" CACHE STRING "The warning flags")

make_directory("bin")
make_directory("lib")

if(NOT WARNING_FLAGS)
	set(WARNING_FLAGS "-Wall -Werror -Wshadow -Wno-uninitialized -Wno-strict-aliasing -Wformat -Wformat-security -Wredundant-decls -Wmissing-noreturn -Winit-self  -Wpointer-arith -Wno-missing-field-initializers")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${WARNING_FLAGS}")

#Handles PKG Config files
find_package(PkgConfig REQUIRED)
PKG_CHECK_MODULES(SERVER_ENGINE  REQUIRED serenoServer)
PKG_CHECK_MODULES(VTK_PARSER     REQUIRED serenoVTKParser)
PKG_CHECK_MODULES(SERENO_SCI_VIS REQUIRED serenoSciVis)
PKG_CHECK_MODULES(SERENO_MATH    REQUIRED serenoMath)

#Search for files
file(GLOB_RECURSE SRCS    src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.h)

#Add an executable to create
include_directories(${CMAKE_SOURCE_DIR}/include/)
link_directories(${VRPN_LIB_DIR} ${QUAT_LIB_DIR})
add_executable(VFVServer ${SRCS} ${HEADERS})
target_compile_options(VFVServer PUBLIC ${SERVER_ENGINE_CFLAGS} ${VTK_PARSER_CFLAGS} ${SERENO_SCI_VIS_CFLAGS} ${SERENO_MATH_CFLAGS} -I${VRPN_INCLUDE_DIR})
target_link_libraries(VFVServer PUBLIC ${SERVER_ENGINE_LDFLAGS} ${VTK_PARSER_LDFLAGS} ${SERENO_SCI_VIS_LDFLAGS} ${SERENO_MATH_LDFLAGS} -lm -lpthread -lvrpn -lquat)
