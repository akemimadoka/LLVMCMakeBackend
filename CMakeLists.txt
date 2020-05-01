cmake_minimum_required(VERSION 3.15)

project(LLVMCMakeBackend CXX)

find_package(LLVM REQUIRED CONFIG)
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

add_subdirectory(lib)
add_subdirectory(src)
