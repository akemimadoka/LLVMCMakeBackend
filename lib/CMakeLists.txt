set(LLVM_LINK_COMPONENTS
	Analysis
	CodeGen
	Core
	MC
	ScalarOpts
	Support
	Target
	SelectionDAG
	TransformUtils
)

file(GLOB_RECURSE SRCS
	*.cpp
	*.h
)

add_llvm_target(CMakeBackendCodeGen
	${SRCS}
)

target_compile_features(LLVMCMakeBackendCodeGen PUBLIC cxx_std_20)
