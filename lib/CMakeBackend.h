#ifndef CMAKE_BACKEND_H
#define CMAKE_BACKEND_H

#include "CMakeTargetMachine.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Transforms/Scalar.h>

namespace LLVMCMakeBackend
{
	class CMakeBackend : public llvm::FunctionPass, public llvm::InstVisitor<CMakeBackend>
	{
		inline static char ID;

		static constexpr unsigned ImplementedIntrinsics[]{ llvm::Intrinsic::memcpy };

	public:
		explicit CMakeBackend(llvm::raw_ostream& outStream)
		    : FunctionPass{ ID }, m_Out{ outStream }, m_CurrentFunction{}, m_CurrentIntent{},
		      m_CurrentTemporaryID{}
		{
		}

		llvm::StringRef getPassName() const override
		{
			return "CMake backend";
		}

		bool doInitialization(llvm::Module& M) override;
		bool doFinalization(llvm::Module& M) override;
		bool runOnFunction(llvm::Function& F) override;

		void visitInstruction(llvm::Instruction& I);

		void visitCastInst(llvm::CastInst& I);

		void visitReturnInst(llvm::ReturnInst& i);

		void visitCallInst(llvm::CallInst& I);
		void visitInlineAsm(llvm::CallInst& I);

		void visitBinaryOperator(llvm::BinaryOperator& I);

		void visitLoadInst(llvm::LoadInst& I);
		void visitStoreInst(llvm::StoreInst& I);

		void visitAllocaInst(llvm::AllocaInst& I);
		void visitGetElementPtrInst(llvm::GetElementPtrInst& I);

		void visitBranchInst(llvm::BranchInst& I);

	private:
		llvm::raw_ostream& m_Out;
		llvm::Function* m_CurrentFunction;
		std::unique_ptr<llvm::DataLayout> m_DataLayout;
		std::unique_ptr<llvm::IntrinsicLowering> m_IntrinsicLowering;

		bool lowerIntrinsics(llvm::Function& f);
		void visitIntrinsics(llvm::CallInst& call);

		std::size_t m_CurrentIntent;
		void outputIntent();

		std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> m_CondElseEndifStack;

		std::unordered_map<llvm::Value*, std::size_t> m_TemporaryID;
		std::size_t m_CurrentTemporaryID;
		std::size_t getTemporaryID(llvm::Value* v);
		std::size_t allocateTemporaryID();
		std::string getTemporaryName(std::size_t id);
		std::string allocateTemporaryName();
		std::string getValueName(llvm::Value* v);
		std::string getFunctionReturnValueName(llvm::Function* v);

		std::unordered_map<llvm::Type*, std::string> m_TypeNameCache;
		llvm::StringRef getTypeName(llvm::Type* type);

		std::unordered_map<llvm::Type*, std::size_t> m_TypeFieldCountCache;
		std::size_t getTypeFieldCount(llvm::Type* type);

		std::unordered_map<llvm::Type*, std::string> m_TypeZeroInitializerCache;
		llvm::StringRef getTypeZeroInitializer(llvm::Type* type);

		void outputFunction(llvm::Function& f);
		void outputBasicBlock(llvm::BasicBlock* bb);

		void evalOperand(llvm::Value* v);
		std::string evalOperand(llvm::Value* v, llvm::StringRef nameHint);
		std::string evalConstant(llvm::Constant* con, llvm::StringRef nameHint = "");

		void outputTypeLayout(llvm::Type* type);

		void outputLoad(llvm::StringRef resultName, llvm::Value* src);
		void outputStore(llvm::Value* value, llvm::Value* dest);
		void outputStore(llvm::StringRef valueName, llvm::Value* dest);
	};
} // namespace LLVMCMakeBackend

#endif