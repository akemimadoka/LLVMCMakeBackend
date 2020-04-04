#include "CMakeBackend.h"

using namespace llvm;
using namespace LLVMCMakeBackend;

bool CMakeBackend::doInitialization(Module& M)
{
	for (auto& global : M.globals())
	{
		evalConstant(global.getInitializer(), global.getName());
	}

	return false;
}

bool CMakeBackend::doFinalization(Module& M)
{
	// TODO: 输出类型布局元数据

	return false;
}

bool CMakeBackend::runOnFunction(Function& F)
{
	m_CurrentFunction = &F;
	m_CurrentIntent = 0;
	m_CurrentTemporaryID = 0;
	m_TemporaryID.clear();

	outputFunction(F);

	return false;
}

void CMakeBackend::GenerateIntrinsics()
{
	assert(m_CurrentIntent == 0);
}

void CMakeBackend::visitReturnInst(ReturnInst& i)
{
	using namespace std::literals::string_literals;

	// CMake 不能返回值，太菜了
	if (i.getNumOperands())
	{
		evalOperand(i.getOperand(0));
		const auto opName = getValueName(i.getOperand(0));
		outputIntent();
		m_Out << "set(" << getFunctionReturnValueName(m_CurrentFunction) << " ${" << opName
		      << "} PARENT_SCOPE)\n";
	}

	outputIntent();
	m_Out << "return()\n";
}

void CMakeBackend::visitCallInst(CallInst& I)
{
	if (isa<InlineAsm>(I.getCalledValue()))
	{
		visitInlineAsm(I);
		return;
	}

	for (std::size_t i = 0; i < I.getNumArgOperands(); ++i)
	{
		evalOperand(I.getArgOperand(i));
	}

	const auto func = I.getCalledFunction();
	outputIntent();
	m_Out << func->getName() << "(";
	for (std::size_t i = 0; i < I.getNumArgOperands(); ++i)
	{
		m_Out << "${" << getValueName(I.getArgOperand(i))
		      << (i == I.getNumArgOperands() - 1 ? "}" : "} ");
	}
	m_Out << ")\n";

	if (!func->getReturnType()->isVoidTy())
	{
		outputIntent();
		m_Out << "set(" << getValueName(&I) << " ${" << getFunctionReturnValueName(func) << "})\n";
	}
}

void CMakeBackend::visitInlineAsm(CallInst& I)
{
	const auto as = cast<InlineAsm>(I.getCalledValue());
	outputIntent();
	m_Out << as->getAsmString() << "\n";
}

void CMakeBackend::visitBinaryOperator(llvm::BinaryOperator& I)
{
	const auto name = getValueName(&I);

	const auto operandLhs = getValueName(I.getOperand(0));
	const auto operandRhs = getValueName(I.getOperand(1));

	evalOperand(I.getOperand(0));
	evalOperand(I.getOperand(1));

	outputIntent();
	m_Out << "math(EXPR " << name << " \"${" << operandLhs << "} ";

	switch (I.getOpcode())
	{
	default:
		assert("Impossible");
		std::terminate();
	case Instruction::Add:
		m_Out << "+";
		break;
	case Instruction::Sub:
		m_Out << "-";
		break;
	case Instruction::Mul:
		m_Out << "*";
		break;
	case Instruction::SDiv:
		m_Out << "/";
		break;
	case Instruction::SRem:
		m_Out << "%";
		break;
	}

	m_Out << " ${" << operandRhs << "}\")\n";
}

void CMakeBackend::visitLoadInst(llvm::LoadInst& I)
{
	const auto operand = getValueName(I.getOperand(0));
	evalOperand(I.getOperand(0));

	const auto destName = getValueName(&I);

	const auto headName = allocateTemporaryName();
	outputIntent();
	m_Out << "list(GET ${" << operand << "} 0 " << headName << ")\n";
	outputIntent();
	m_Out << "if(${" << headName << "} STREQUAL \"_LLVM_CMAKE_GEP_\")\n";
	outputIntent();
	const auto listRef = allocateTemporaryName();
	m_Out << "\tlist(GET ${" << operand << "} 1 " << listRef << ")\n";
	outputIntent();
	const auto idxValue = allocateTemporaryName();
	m_Out << "\tlist(GET ${" << operand << "} 2 " << idxValue << ")\n";
	outputIntent();
	m_Out << "\tlist(GET ${${" << listRef << "}} ${" << idxValue << "} " << destName << ")\n";
	outputIntent();
	m_Out << "else()\n";
	outputIntent();
	m_Out << "\tset(" << destName << " ${${" << operand << "}})\n";
	outputIntent();
	m_Out << "endif()\n";
}

void CMakeBackend::visitStoreInst(llvm::StoreInst& I)
{
	const auto operandValue = getValueName(I.getOperand(0));
	evalOperand(I.getOperand(0));

	const auto operandDest = getValueName(I.getOperand(1));
	evalOperand(I.getOperand(1));

	const auto headName = allocateTemporaryName();
	outputIntent();
	m_Out << "list(GET ${" << operandDest << "} 0 " << headName << ")\n";
	outputIntent();
	m_Out << "if(${" << headName << "} STREQUAL \"_LLVM_CMAKE_GEP_\")\n";
	outputIntent();
	const auto listRef = allocateTemporaryName();
	m_Out << "\tlist(GET ${" << operandDest << "} 1 " << listRef << ")\n";
	outputIntent();
	const auto idxValue = allocateTemporaryName();
	m_Out << "\tlist(GET ${" << operandDest << "} 2 " << idxValue << ")\n";
	outputIntent();
	m_Out << "\tlist(REMOVE_AT ${" << listRef << "} ${" << idxValue << "})\n";
	outputIntent();
	m_Out << "\tlist(INSERT ${" << listRef << "} ${" << idxValue << "} ${" << operandValue << "})\n";
	outputIntent();
	m_Out << "else()\n";
	outputIntent();
	m_Out << "\tset(${" << operandDest << "} ${" << operandValue << "})\n";
	outputIntent();
	m_Out << "endif()\n";
}

void CMakeBackend::visitAllocaInst(llvm::AllocaInst& I)
{
	auto elemType = I.getAllocatedType();
	assert(elemType == I.getType()->getPointerElementType());

	const auto elemSize = I.getArraySize();
	if (const auto constSize = dyn_cast<ConstantInt>(elemSize))
	{
		const auto size = constSize->getValue().getZExtValue();

		const auto objectName = allocateTemporaryName();

		outputIntent();
		m_Out << "set(" << objectName << " \"";
		for (std::size_t i = 0; i < size; ++i)
		{
			outputTypeLayout(elemType);
		}
		m_Out << "\")\n";

		// 结果为指针
		outputIntent();
		m_Out << "set(" << getValueName(&I) << " " << objectName << ")\n";
	}
	else
	{
		assert(!"Not implemented");
		std::terminate();
	}
}

void CMakeBackend::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
	if (I.getNumOperands() == 0)
	{
		assert(!"Not implemented");
		std::terminate();
	}

	const auto ptrOperand = I.getPointerOperand();
	const auto ptrType = cast<PointerType>(ptrOperand->getType());
	evalOperand(ptrOperand);

	const auto listName = getValueName(ptrOperand);

	std::size_t realIdx{};
	auto pointeeType = ptrType->getElementType();
	for (std::size_t i = 2; i < I.getNumOperands(); ++i)
	{
		const auto idxOperand = I.getOperand(i);
		// TODO: 支持动态索引
		if (const auto constIdx = dyn_cast<ConstantInt>(idxOperand))
		{
			const auto idxValue = constIdx->getValue().getZExtValue();
			if (const auto arrayType = dyn_cast<ArrayType>(pointeeType))
			{
				const auto fieldSize = getTypeFieldCount(arrayType->getElementType());
				realIdx += idxValue * fieldSize;
				pointeeType = arrayType->getElementType();
			}
			else if (const auto structType = dyn_cast<StructType>(pointeeType))
			{
				for (std::size_t i = 0; i < idxValue; ++i)
				{
					const auto fieldType = structType->getElementType(i);
					realIdx += getTypeFieldCount(fieldType);
				}
				pointeeType = structType->getElementType(idxValue);
			}
			else
			{
				assert(!"Unsupported type");
				std::terminate();
			}
		}
		else
		{
			assert(!"Not implemented");
			std::terminate();
		}
	}

	outputIntent();
	m_Out << "set(" << getValueName(&I) << " \"_LLVM_CMAKE_GEP_;" << listName << ";" << realIdx
	      << "\")\n";
}

void CMakeBackend::visitBranchInst(BranchInst& I)
{
	if (I.isConditional())
	{
		const auto cond = I.getCondition();
		evalOperand(cond);
		const auto condName = getValueName(cond);
		outputIntent();
		m_Out << "if(${" << condName << "})\n";
		const auto trueBranch = I.getSuccessor(0);
		const auto falseBranch = I.getSuccessor(1);
		if (I.getParent()->getNextNode() != trueBranch)
		{
			assert(!"Goto not supported in CMake.");
			std::terminate();
		}

		// TODO: 处理 else if
		if (const auto endIfBr = dyn_cast<BranchInst>(falseBranch->getPrevNode()->getTerminator());
		    endIfBr && endIfBr->getNumSuccessors() == 1)
		{
			const auto endIfBlock = endIfBr->getSuccessor(0);
			++m_CurrentIntent;
			m_CondElseEndifStack.push_back({ I.getSuccessor(1), endIfBlock });
		}
		else
		{
			assert(!"Goto not supported in CMake.");
			std::terminate();
		}
	}
	else
	{
		const auto successor = I.getSuccessor(0);
		const auto next = I.getParent()->getNextNode();
		if (successor != next)
		{
			if (m_CondElseEndifStack.empty() || m_CondElseEndifStack.back().second != successor)
			{
				assert(!"Goto not supported in CMake.");
				std::terminate();
			}
		}
	}
}

void CMakeBackend::outputIntent()
{
	for (std::size_t i = 0; i < m_CurrentIntent; ++i)
	{
		m_Out << "\t";
	}
}

std::size_t CMakeBackend::getTemporaryID(llvm::Value* v)
{
	auto iter = m_TemporaryID.find(v);
	if (iter == m_TemporaryID.end())
	{
		std::tie(iter, std::ignore) = m_TemporaryID.emplace(v, m_CurrentTemporaryID++);
	}

	return iter->second;
}

std::size_t CMakeBackend::allocateTemporaryID()
{
	return m_CurrentTemporaryID++;
}

std::string CMakeBackend::getTemporaryName(std::size_t id)
{
	return "_LLVM_CMAKE_TEMP_" + std::to_string(id);
}

std::string CMakeBackend::allocateTemporaryName()
{
	return getTemporaryName(allocateTemporaryID());
}

std::string CMakeBackend::getValueName(llvm::Value* v)
{
	if (v->hasName())
	{
		return static_cast<std::string>(v->getName());
	}

	return getTemporaryName(getTemporaryID(v));
}

std::string CMakeBackend::getFunctionReturnValueName(llvm::Function* v)
{
	return "_LLVM_CMAKE_" + getValueName(v) + "_RETURN_VALUE";
}

llvm::StringRef CMakeBackend::getTypeName(llvm::Type* type)
{
	using namespace std::literals::string_literals;

	auto iter = m_TypeNameCache.find(type);
	if (iter == m_TypeNameCache.end())
	{
		std::string typeName;
		if (type->isStructTy())
		{
			typeName = type->getStructName();
		}
		else if (const auto intType = dyn_cast<IntegerType>(type))
		{
			switch (intType->getBitWidth())
			{
			case 1:
				typeName = "bool";
				break;
			case 8:
				typeName = "char";
				break;
			case 16:
			case 32:
			case 64:
			case 128:
				typeName = "Interger";
				break;
			}
		}
		else if (type->isPointerTy())
		{
			typeName = getTypeName(type->getPointerElementType());
			typeName += "*";
		}
		else if (type->isArrayTy())
		{
			typeName = getTypeName(type->getArrayElementType());
			typeName += "[";
			typeName += std::to_string(type->getArrayNumElements());
			typeName += "]";
		}
		else
		{
			assert(!"Unsupported type.");
			std::terminate();
		}

		std::tie(iter, std::ignore) = m_TypeNameCache.emplace(type, std::move(typeName));
	}

	return iter->second;
}

std::size_t CMakeBackend::getTypeFieldCount(llvm::Type* type)
{
	auto iter = m_TypeFieldCountCache.find(type);
	if (iter == m_TypeFieldCountCache.end())
	{
		std::size_t result;
		if (const auto structType = dyn_cast<StructType>(type))
		{
			for (std::size_t i = 0; i < structType->getNumElements(); ++i)
			{
				result += getTypeFieldCount(structType->getElementType(i));
			}
		}
		else if (type->isArrayTy())
		{
			result = type->getArrayNumElements() * getTypeFieldCount(type->getArrayElementType());
		}
		else
		{
			result = 1;
		}

		std::tie(iter, std::ignore) = m_TypeFieldCountCache.emplace(type, result);
	}

	return iter->second;
}

void CMakeBackend::outputFunction(Function& f)
{
	m_Out << "function(" << f.getName();
	for (auto& arg : f.args())
	{
		m_Out << " " << getValueName(&arg);
	}
	m_Out << ")\n";

	for (auto& bb : f)
	{
		outputBasicBlock(&bb);
	}

	m_Out << "endfunction()\n\n";
}

void CMakeBackend::outputBasicBlock(BasicBlock* bb)
{
	if (!m_CondElseEndifStack.empty())
	{
		if (const auto [elseBlock, endIfBlock] = m_CondElseEndifStack.back();
		    bb == elseBlock && bb != endIfBlock)
		{
			outputIntent();
			m_Out << "else()\n";
		}
		else if (bb == endIfBlock)
		{
			outputIntent();
			m_Out << "endif()\n";
			m_CondElseEndifStack.pop_back();
			--m_CurrentIntent;
		}
	}

	++m_CurrentIntent;
	for (auto& ins : *bb)
	{
		visit(ins);
	}
	--m_CurrentIntent;
}

void CMakeBackend::evalOperand(llvm::Value* v)
{
	if (const auto con = dyn_cast<Constant>(v); con && !isa<GlobalValue>(con))
	{
		evalConstant(con);
	}
}

std::string CMakeBackend::evalOperand(llvm::Value* v, llvm::StringRef nameHint)
{
	if (const auto con = dyn_cast<Constant>(v); con && !isa<GlobalValue>(con))
	{
		return evalConstant(con, nameHint);
	}
	else
	{
		// 是变量引用
		return nameHint.empty() ? getValueName(v) : static_cast<std::string>(nameHint);
	}
}

std::string CMakeBackend::evalConstant(llvm::Constant* con, llvm::StringRef nameHint)
{
	auto conName = nameHint.empty() ? getValueName(con) : static_cast<std::string>(nameHint);

	if (const auto ce = dyn_cast<ConstantExpr>(con))
	{
		switch (ce->getOpcode())
		{
		case Instruction::Add:
		case Instruction::Sub:
		case Instruction::Mul:
		case Instruction::SDiv:
		case Instruction::SRem: {
			visitBinaryOperator(*cast<llvm::BinaryOperator>(ce));
			return conName;
		}

		default:
			errs() << "Err!\n";
			std::terminate();
		}
	}

	if (const auto i = dyn_cast<ConstantInt>(con))
	{
		outputIntent();
		m_Out << "set(" << conName << " \"" << i->getValue() << "\")\n";
		return conName;
	}
	else if (const auto arr = dyn_cast<ConstantArray>(con))
	{
		outputIntent();
		m_Out << "set(" << conName << " \"";
		const auto elemType = cast<ArrayType>(arr->getType())->getElementType();
		if (elemType->isIntegerTy(8))
		{
			// 假设是字符串
			for (std::size_t i = 0; i < arr->getNumOperands(); ++i)
			{
				m_Out << static_cast<char>(
				    cast<ConstantInt>(arr->getOperand(i))->getValue().getZExtValue());
			}
		}
		else if (elemType->isIntegerTy())
		{
			for (std::size_t i = 0; i < arr->getNumOperands(); ++i)
			{
				m_Out << cast<ConstantInt>(arr->getOperand(i))->getValue().getZExtValue();
				if (i != arr->getNumOperands() - 1)
				{
					m_Out << ";";
				}
			}
		}
		m_Out << "\")\n";
		return conName;
	}
	else if (const auto seq = dyn_cast<ConstantDataSequential>(con))
	{
		outputIntent();
		m_Out << "set(" << conName << " \"";
		const auto elemType = cast<ArrayType>(seq->getType())->getElementType();
		if (elemType->isIntegerTy(8))
		{
			// 假设是字符串，去除结尾0
			for (std::size_t i = 0; i < seq->getNumElements() - 1; ++i)
			{
				m_Out << static_cast<char>(
				    cast<ConstantInt>(seq->getElementAsConstant(i))->getValue().getZExtValue());
			}
		}
		else if (elemType->isIntegerTy())
		{
			for (std::size_t i = 0; i < seq->getNumElements(); ++i)
			{
				m_Out << cast<ConstantInt>(seq->getElementAsConstant(i))->getValue().getZExtValue();
				if (i != seq->getNumElements() - 1)
				{
					m_Out << ";";
				}
			}
		}
		m_Out << "\")\n";
		return conName;
	}

	errs() << "Err!\n";
	std::terminate();
}

void CMakeBackend::outputTypeLayout(llvm::Type* type)
{
	if (const auto structType = dyn_cast<StructType>(type))
	{
		for (std::size_t i = 0; i < structType->getNumElements(); ++i)
		{
			outputTypeLayout(structType->getElementType(i));
			if (i != structType->getNumElements() - 1)
			{
				m_Out << ";";
			}
		}
	}
	else if (const auto arrayType = dyn_cast<ArrayType>(type))
	{
		const auto elemType = arrayType->getElementType();
		for (std::size_t i = 0; i < arrayType->getArrayNumElements(); ++i)
		{
			outputTypeLayout(elemType);
			if (i != arrayType->getArrayNumElements() - 1)
			{
				m_Out << ";";
			}
		}
	}
	else
	{
		m_Out << getTypeName(type);
	}
}
