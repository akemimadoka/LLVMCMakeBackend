#include "CMakeTargetMachine.h"
#include "CMakeBackend.h"
#include "CheckNeedGotoPass.h"

#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Transforms/Utils.h>

using namespace llvm;

Target CMakeBackendTarget{};

extern "C" void LLVMInitializeCMakeBackendTarget()
{
	RegisterTargetMachine<LLVMCMakeBackend::CMakeTargetMachine> X(CMakeBackendTarget);
}

extern "C" void LLVMInitializeCMakeBackendTargetInfo()
{
	RegisterTarget<> X(CMakeBackendTarget, "cmake", "CMake backend", "CMake");
}

extern "C" void LLVMInitializeCMakeBackendTargetMC()
{
}

bool LLVMCMakeBackend::CMakeTargetSubtargetInfo::enableAtomicExpand() const
{
	return true;
}

const llvm::TargetLowering* LLVMCMakeBackend::CMakeTargetSubtargetInfo::getTargetLowering() const
{
	return &Lowering;
}

bool LLVMCMakeBackend::CMakeTargetMachine::addPassesToEmitFile(
    llvm::PassManagerBase& PM, llvm::raw_pwrite_stream& Out, llvm::raw_pwrite_stream* DwoOut,
    llvm::CodeGenFileType FileType, bool DisableVerify, llvm::MachineModuleInfoWrapperPass* MMIWP)
{
	if (FileType != CodeGenFileType::CGFT_AssemblyFile)
	{
		return true;
	}

	const auto reg = PassRegistry::getPassRegistry();

	const auto passConfig = createPassConfig(PM);
	PM.add(passConfig);
	PM.add(createLowerSwitchPass());
	PM.add(new CheckNeedGotoPass());
	PM.add(new CMakeBackend(Out));

	return false;
}

const llvm::TargetSubtargetInfo*
LLVMCMakeBackend::CMakeTargetMachine::getSubtargetImpl(const Function&) const
{
	return &SubtargetInfo;
}
