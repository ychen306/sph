cmake_minimum_required(VERSION 3.4)
project(SPH)

string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE
               "${CMAKE_CXX_FLAGS_RELEASE}")

include(sanitizers.cmake)

add_library(SPH MODULE SpeculativeLoadHoisting.cpp)

if(APPLE)
  set_target_properties(SPH PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LLVM_BUILD_TOOLS ON)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(-fno-rtti)
add_definitions(-std=c++14)

configure_file(${CMAKE_SOURCE_DIR}/test/lit.site.cfg.in
               ${CMAKE_BINARY_DIR}/test/lit.site.cfg)

configure_file(${CMAKE_SOURCE_DIR}/utils/sph-clang.in
               ${CMAKE_BINARY_DIR}/sph-clang)

configure_file(${CMAKE_SOURCE_DIR}/utils/sph-clang.in
               ${CMAKE_BINARY_DIR}/sph-clang++)

configure_file(${CMAKE_SOURCE_DIR}/utils/sph-opt.in
               ${CMAKE_BINARY_DIR}/sph-opt)
