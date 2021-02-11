#include "CheckNeedGotoPass.h"

#include <llvm/IR/Instructions.h>

#include <unordered_set>

using namespace LLVMCMakeBackend;

StateInfo::StateInfo(std::vector<const llvm::BasicBlock*> bbList) : m_BBList{ std::move(bbList) }
{
}

void StateInfo::Init()
{
	m_CurBB = m_BBList.front();
	m_CurBBStateID = 0;
}

std::size_t StateInfo::GetNextStateID()
{
    if (!m_CurBB)
    {
        return -1;
    }

	m_CurBB = m_CurBB->getNextNode();

    if (!m_CurBB)
    {
        return -1;
    }

	if (m_CurBBStateID < m_BBList.size() - 1 && m_CurBB == m_BBList[m_CurBBStateID + 1])
	{
		++m_CurBBStateID;
	}
	return m_CurBBStateID;
}

std::size_t StateInfo::GetStateID(const llvm::BasicBlock* bb)
{
    const auto iter = std::find(m_BBList.begin(), m_BBList.end(), bb);
    if (iter == m_BBList.end())
    {
        return -1;
    }

    return iter - m_BBList.begin();
}

char CheckNeedGotoPass::ID{};

CheckNeedGotoPass::CheckNeedGotoPass() : FunctionPass(ID)
{
}

bool CheckNeedGotoPass::runOnFunction(llvm::Function& f)
{
	if (checkNeedGoto(f))
	{
		buildState(f);
	}

	return false;
}

void CheckNeedGotoPass::releaseMemory()
{
	m_BBList.clear();
}

bool CheckNeedGotoPass::isCurrentFunctionNeedGoto() const noexcept
{
	return !m_BBList.empty();
}

StateInfo CheckNeedGotoPass::getCurrentStateInfo() const noexcept
{
	return { m_BBList };
}

bool CheckNeedGotoPass::checkNeedGoto(llvm::Function& f) const
{
	std::vector<llvm::BasicBlock*> elseStack;
	std::vector<llvm::BasicBlock*> endIfStack;
	for (const auto& bb : f)
	{
		const auto terminator = bb.getTerminator();
		if (const auto br = llvm::dyn_cast<llvm::BranchInst>(terminator))
		{
            // 若 br 出现在函数结尾的 bb 里，则其一定是前向的，因此一定需要 goto
            if (&bb == &f.back())
            {
                return true;
            }

			const auto next = bb.getNextNode();
			if (br->isUnconditional())
			{
				const auto successor = br->getSuccessor(0);
				// 非 fall through，检查是否是 if-else 的非 else 分支，若是，则 endif 应是跳转到的分支而不是
				// else 分支
				if (successor != next)
				{
					if (!elseStack.empty() && next == elseStack.back() && !endIfStack.empty() &&
					    next == endIfStack.back())
					{
						endIfStack.back() = successor;
					}
					else
					{
						return true;
					}
				}
				else
				{
					while (!endIfStack.empty() && endIfStack.back() == next)
					{
						endIfStack.pop_back();
					}
				}

				continue;
			}

			// 是条件跳转
			assert(br->isConditional());

			const auto branch1 = br->getSuccessor(0), branch2 = br->getSuccessor(1);
			if (branch1 != next && branch2 != next)
			{
				return true;
			}

			const auto elseBranch = branch1 == next ? branch2 : branch1;
			elseStack.push_back(elseBranch);
			// 先假设是 endif（即简单 if 形式）
			endIfStack.push_back(elseBranch);
		}
	}

	return !elseStack.empty() || !endIfStack.empty();
}

void CheckNeedGotoPass::buildState(llvm::Function& f)
{
	std::unordered_set<const llvm::BasicBlock*> jumpedToBB{ &f.front() };
	for (const auto& bb : f)
	{
		m_BBList.push_back(&bb);
		const auto terminator = bb.getTerminator();
		if (const auto br = llvm::dyn_cast<llvm::BranchInst>(terminator))
		{
			const auto next = bb.getNextNode();
			const auto successorCount = br->getNumSuccessors();
			for (std::size_t i = 0; i < successorCount; ++i)
			{
				if (const auto successor = br->getSuccessor(i); successor != next)
				{
					jumpedToBB.emplace(successor);
				}
			}
		}
	}
	std::erase_if(m_BBList, [&](const llvm::BasicBlock* bb) { return !jumpedToBB.contains(bb); });
}
