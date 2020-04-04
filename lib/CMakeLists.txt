set(LLVM_LINK_COMPONENTS
	Analysis
	CodeGen
	Core
	MC
	ScalarOpts
	Support
	Target
	SelectionDAG
	TransformUtils)

add_llvm_target(CMakeBackendCodeGen
	CMakeBackend.cpp
	CMakeTargetMachine.cpp)

target_compile_features(LLVMCMakeBackendCodeGen PUBLIC cxx_std_20)
