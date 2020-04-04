#ifndef CMAKE_TARGET_MACHINE_H
#define CMAKE_TARGET_MACHINE_H

#include <llvm/CodeGen/TargetLowering.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Target/TargetMachine.h>

extern llvm::Target CMakeBackendTarget;
extern "C" void LLVMInitializeCMakeBackendTarget();
extern "C" void LLVMInitializeCMakeBackendTargetInfo();
extern "C" void LLVMInitializeCMakeBackendTargetMC();

namespace LLVMCMakeBackend
{
	class CMakeTargetLowering : public llvm::TargetLowering
	{
	public:
		explicit CMakeTargetLowering(const llvm::TargetMachine& TM) : TargetLowering(TM)
		{
			setMaxAtomicSizeInBitsSupported(0);
		}
	};

	class CMakeTargetSubtargetInfo : public llvm::TargetSubtargetInfo
	{
	public:
		CMakeTargetSubtargetInfo(const llvm::TargetMachine& TM, const llvm::Triple& TT,
		                         llvm::StringRef CPU, llvm::StringRef FS)
		    : TargetSubtargetInfo(TT, CPU, FS, {}, {}, nullptr, nullptr, nullptr, nullptr, nullptr,
		                          nullptr),
		      Lowering(TM)
		{
		}
		bool enableAtomicExpand() const override;
		const llvm::TargetLowering* getTargetLowering() const override;

	private:
		CMakeTargetLowering Lowering;
	};

	class CMakeTargetMachine : public llvm::LLVMTargetMachine
	{
	public:
		CMakeTargetMachine(const llvm::Target& T, const llvm::Triple& TT, llvm::StringRef CPU,
		                   llvm::StringRef FS, const llvm::TargetOptions& Options,
		                   llvm::Optional<llvm::Reloc::Model> RM,
		                   llvm::Optional<llvm::CodeModel::Model> CM, llvm::CodeGenOpt::Level OL,
		                   bool /*JIT*/)
		    : LLVMTargetMachine(T, "", TT, CPU, FS, Options, RM.getValueOr(llvm::Reloc::Static),
		                        CM.getValueOr(llvm::CodeModel::Small), OL),
		      SubtargetInfo(*this, TT, CPU, FS)
		{
		}

		bool addPassesToEmitFile(llvm::PassManagerBase& PM, llvm::raw_pwrite_stream& Out,
		                         llvm::raw_pwrite_stream* DwoOut, llvm::CodeGenFileType FileType,
		                         bool DisableVerify = true,
		                         llvm::MachineModuleInfoWrapperPass* MMIWP = nullptr) override;

		const llvm::TargetSubtargetInfo* getSubtargetImpl(const llvm::Function&) const override;

	private:
		CMakeTargetSubtargetInfo SubtargetInfo;
	};
} // namespace LLVMCMakeBackend

#endif
