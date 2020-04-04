#include <CMakeBackend.h>

#include <iostream>
#include <memory>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>

using namespace llvm;

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Wrong arg." << std::endl;
		return 1;
	}

	llvm_shutdown_obj llvm_shutdown;

	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmPrinters();
	InitializeAllAsmParsers();

	LLVMInitializeCMakeBackendTarget();
	LLVMInitializeCMakeBackendTargetInfo();
	LLVMInitializeCMakeBackendTargetMC();

	LLVMContext context;

	const std::string inputFile = argv[1];
	SMDiagnostic err;
	const auto mod = parseIRFile(inputFile, err, context);
	if (!mod)
	{
		err.print(argv[0], errs());
		return 1;
	}

	Triple triple{ sys::getDefaultTargetTriple() };

	std::string errStr;
	const auto target = TargetRegistry::lookupTarget("cmake", triple, errStr);
	if (!target)
	{
		std::cerr << errStr << std::endl;
		return 1;
	}

	TargetOptions options{};
	const auto targetMachine = target->createTargetMachine(triple.getTriple(), "", "", options, None);
	if (!targetMachine)
	{
		std::cerr << "Failed to create target machine" << std::endl;
		return 1;
	}

	std::error_code errc;
	ToolOutputFile out(inputFile + ".cmake", errc, sys::fs::F_Text);
	if (errc)
	{
		std::cerr << errc.message() << std::endl;
		return 1;
	}

	legacy::PassManager pm;
	if (targetMachine->addPassesToEmitFile(pm, out.os(), nullptr, CodeGenFileType::CGFT_AssemblyFile,
	                                       true, nullptr))
	{
		std::cerr << "Emit file error" << std::endl;
	}

	pm.run(*mod);

	out.keep();
}
