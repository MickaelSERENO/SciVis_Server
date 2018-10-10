#Version of cmake
cmake_minimum_required(VERSION 3.0)

#Name of the Project
project(Server_Vector_Field_Visualization)

#Set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY   ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)

SET(WARNING_FLAGS "" CACHE STRING "The warning flags")

make_directory("bin")
make_directory("lib")

if(NOT WARNING_FLAGS)
	set(WARNING_FLAGS "-Wall -Werror -Wshadow -Wno-uninitialized -Wno-strict-aliasing -Wformat -Wformat-security -Wredundant-decls -Wmissing-noreturn -Winit-self  -Wpointer-arith -Wno-missing-field-initializers")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${WARNING_FLAGS}")

find_package(PkgConfig REQUIRED)

#Search for files
file(GLOB_RECURSE SRCS    src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.h)

#Add an executable to create
add_executable(VRCamera ${SRCS} ${HEADERS})
