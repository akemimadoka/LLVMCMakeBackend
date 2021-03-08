file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
	*.cpp)

add_executable(LLVMCMakeBackend ${SOURCE_FILES})

llvm_map_components_to_libnames(libs
	CMakeBackendCodeGen
)

target_link_libraries(LLVMCMakeBackend PRIVATE LLVM ${libs})
target_include_directories(LLVMCMakeBackend PRIVATE ${CMAKE_SOURCE_DIR}/lib)
