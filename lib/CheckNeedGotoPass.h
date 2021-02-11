#ifndef CHECK_NEED_GOTO_PASS_H
#define CHECK_NEED_GOTO_PASS_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>

namespace LLVMCMakeBackend
{
	// 包含必须 goto 的指令的函数体将是如下：
	// # function prolog
	// set(_LLVM_CMAKE_LOOP_STATE 0)
	// while(1)
	//   if(_LLVM_CMAKE_LOOP_STATE STREQUAL "0")
	//     # do sth
	//     set(_LLVM_CMAKE_LOOP_STATE "1")
    //   endif()
	//   if(_LLVM_CMAKE_LOOP_STATE STREQUAL "1")
	//     # do sth2
	//     set(_LLVM_CMAKE_LOOP_STATE "3")
    //     continue()
    //   endif()
	//   if(_LLVM_CMAKE_LOOP_STATE STREQUAL "2")
	//     ...
	//   endif()
	// endwhile()
	// 暂无 function epilog，因此不需要循环最终退出，但如果之后添加了，需要进行修改
	class StateInfo
	{
	public:
		StateInfo(std::vector<const llvm::BasicBlock*> bbList);

		void Init();
		std::size_t GetNextStateID();
        // 仅对 section 头部返回 id，若非头部返回 -1
        std::size_t GetStateID(const llvm::BasicBlock* bb);

	private:
		mutable const llvm::BasicBlock* m_CurBB;
		mutable std::size_t m_CurBBStateID;
		// 各 section 的头部（被跳转到的块）
		std::vector<const llvm::BasicBlock*> m_BBList;
	};

	class CheckNeedGotoPass : public llvm::FunctionPass
	{
	public:
		CheckNeedGotoPass();

		bool runOnFunction(llvm::Function& f) override;
		void releaseMemory() override;

		bool isCurrentFunctionNeedGoto() const noexcept;
		StateInfo getCurrentStateInfo() const noexcept;

        static char ID;

	private:
		std::vector<const llvm::BasicBlock*> m_BBList;

		bool checkNeedGoto(llvm::Function& f) const;
		void buildState(llvm::Function& f);
	};
} // namespace LLVMCMakeBackend

#endif
