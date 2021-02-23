#include "CMakeBackend.h"

#include "CheckNeedGotoPass.h"

#include <llvm/Transforms/Utils.h>

#include <charconv>

using namespace llvm;
using namespace LLVMCMakeBackend;

// 所有以 _LLVM_CMAKE_ 开头的标识符都为本实现保留，用户不得使用，因此可以假设此类命名不会和用户冲突
// 定义这类标识符为 丑命名

namespace
{
	// NOTE:
	// 涉及指针的操作时，所有内部实体名称必须为丑命名，因为间接访问时可能获得的是函数内实体而不是外部的

	// 指针可能为胖指针，其他情况都为引用实体的名称

	// 指针的定义如下：
	// pointer = fat-pointer | identifier
	// fat-pointer = pointer-head, property-list, info-list
	// pointer-head = "_LLVM_CMAKE_PTR"
	// property-list = { ".", property }
	// property = "NUL" | "GEP" | "EXT"
	// info-list = { ":", info }
	// info = <any char except ":">

	// 目前包含的属性有 NUL 表示空指针， GEP 表示 gep 而来的指针，以及 EXT 表示引用外部实体的指针
	// info 按照 property 顺序排列
	// 对于 NUL，没有 info
	// 对于 GEP，info 有 2 个，按顺序为偏移和引用的实体名称
	// 对于 EXT，若指针不为 GEP，info 有 1 个，是引用的实体的名称，否则仅作为标记，不包含 info

	// 当前的实现偷懒，并未完全实现上述描述（

	constexpr const char CMakeIntrinsics[] =
	    R"CMakeIntrinsics(# Start of LLVM CMake intrinsics

set(_LLVM_CMAKE_CURRENT_DEPTH "0")

function(_LLVM_CMAKE_EVAL)
	cmake_parse_arguments(ARGUMENT "NO_LOCK" "" "PATH;CONTENT" ${ARGN})
	if(NOT DEFINED ARGUMENT_CONTENT)
		message(FATAL_ERROR "No content provided")
	endif()
	if(NOT DEFINED ARGUMENT_PATH)
		string(MD5 CONTENT_HASH "${ARGUMENT_CONTENT}")
		set(ARGUMENT_PATH "${CMAKE_CURRENT_BINARY_DIR}/_LLVM_CMAKE_EvalCache/${CONTENT_HASH}.cmake")
	endif()
	if(NOT ARGUMENT_NO_LOCK)
		file(LOCK "${ARGUMENT_PATH}.lock" GUARD FUNCTION RESULT_VARIABLE _lock_result)
		if(NOT _lock_result EQUAL 0)
			message(FATAL_ERROR "Cannot lock file \"${ARGUMENT_PATH}.lock\", result code is ${_lock_result}")
		endif()
	endif()
	file(WRITE ${ARGUMENT_PATH} "${ARGUMENT_CONTENT}")
	include(${ARGUMENT_PATH})
endfunction()

macro(_LLVM_CMAKE_PROPAGATE_FUNCTION_RESULT _LLVM_CMAKE_PROPAGATE_FUNCTION_RESULT_FUNC_NAME)
	set(_LLVM_CMAKE_RETURN_VALUE ${_LLVM_CMAKE_RETURN_VALUE} PARENT_SCOPE)
	set(_LLVM_CMAKE_RETURN_MODIFIED_EXTERNAL_VARIABLE_LIST ${_LLVM_CMAKE_RETURN_MODIFIED_EXTERNAL_VARIABLE_LIST} PARENT_SCOPE)
endmacro()

function(_LLVM_CMAKE_INVOKE_FUNCTION_PTR _LLVM_CMAKE_INVOKE_FUNCTION_PTR_FUNC_PTR)
	if(_LLVM_CMAKE_INVOKE_FUNCTION_PTR_FUNC_PTR STREQUAL "_LLVM_CMAKE_PTR.NUL")
		message(FATAL_ERROR "Function pointer is null")
	endif()
	_LLVM_CMAKE_EVAL(CONTENT "${_LLVM_CMAKE_INVOKE_FUNCTION_PTR_FUNC_PTR}(${ARGN})
_LLVM_CMAKE_PROPAGATE_FUNCTION_RESULT(${_LLVM_CMAKE_INVOKE_FUNCTION_PTR_FUNC_PTR})")
	_LLVM_CMAKE_PROPAGATE_FUNCTION_RESULT(${_LLVM_CMAKE_INVOKE_FUNCTION_PTR_FUNC_PTR})
endfunction()

function(_LLVM_CMAKE_CONSTRUCT_GEP _LLVM_CMAKE_CONSTRUCT_GEP_OFFSET _LLVM_CMAKE_CONSTRUCT_GEP_ENTITY_PTR _LLVM_CMAKE_CONSTRUCT_GEP_RESULT_NAME)
	if(_LLVM_CMAKE_CONSTRUCT_GEP_ENTITY_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.NUL.*$")
		message(FATAL_ERROR "Trying to construct gep from a null pointer")
	elseif(_LLVM_CMAKE_CONSTRUCT_GEP_ENTITY_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):(.+)$")
		set(_LLVM_CMAKE_CONSTRUCT_GEP_ORIGIN_OFFSET ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_CONSTRUCT_GEP_REF_ENTITY ${CMAKE_MATCH_2})
		math(EXPR _LLVM_CMAKE_CONSTRUCT_GEP_OFFSET "${_LLVM_CMAKE_CONSTRUCT_GEP_OFFSET} + ${_LLVM_CMAKE_CONSTRUCT_GEP_ORIGIN_OFFSET}")
		set(_LLVM_CMAKE_CONSTRUCT_GEP_RESULT "_LLVM_CMAKE_PTR.GEP.EXT:${_LLVM_CMAKE_CONSTRUCT_GEP_OFFSET}:${_LLVM_CMAKE_CONSTRUCT_GEP_REF_ENTITY}")
	elseif(_LLVM_CMAKE_CONSTRUCT_GEP_ENTITY_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.EXT.*:(.+)$")
		set(_LLVM_CMAKE_CONSTRUCT_GEP_REAL_PTR ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_CONSTRUCT_GEP_RESULT "_LLVM_CMAKE_PTR.GEP.EXT:${_LLVM_CMAKE_CONSTRUCT_GEP_OFFSET}:${_LLVM_CMAKE_CONSTRUCT_GEP_REAL_PTR}")
	else()
		set(_LLVM_CMAKE_CONSTRUCT_GEP_RESULT "_LLVM_CMAKE_PTR.GEP:${_LLVM_CMAKE_CONSTRUCT_GEP_OFFSET}:${_LLVM_CMAKE_CONSTRUCT_GEP_ENTITY_PTR}")
	endif()
	set(${_LLVM_CMAKE_CONSTRUCT_GEP_RESULT_NAME} ${_LLVM_CMAKE_CONSTRUCT_GEP_RESULT} PARENT_SCOPE)
endfunction()

function(_LLVM_CMAKE_LOAD _LLVM_CMAKE_LOAD_PTR _LLVM_CMAKE_LOAD_RESULT_NAME)
	if(_LLVM_CMAKE_LOAD_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.NUL.*$")
		message(FATAL_ERROR "Trying to dereference a null pointer")
	elseif(_LLVM_CMAKE_LOAD_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):(.+)$")
		set(_LLVM_CMAKE_LOAD_OFFSET ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_LOAD_REF_ENTITY ${CMAKE_MATCH_2})
		list(GET ${_LLVM_CMAKE_LOAD_REF_ENTITY} ${_LLVM_CMAKE_LOAD_OFFSET} _LLVM_CMAKE_LOAD_RESULT)
	elseif(_LLVM_CMAKE_LOAD_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.EXT.*:(.+)$")
		set(_LLVM_CMAKE_LOAD_REAL_PTR ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_LOAD_RESULT ${${_LLVM_CMAKE_LOAD_REAL_PTR}})
	else()
		set(_LLVM_CMAKE_LOAD_RESULT ${${_LLVM_CMAKE_LOAD_PTR}})
	endif()
	set(${_LLVM_CMAKE_LOAD_RESULT_NAME} ${_LLVM_CMAKE_LOAD_RESULT} PARENT_SCOPE)
endfunction()

function(_LLVM_CMAKE_STORE _LLVM_CMAKE_STORE_FUNC_MODLIST _LLVM_CMAKE_STORE_PTR)
	if(_LLVM_CMAKE_STORE_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.NUL.*$")
		message(FATAL_ERROR "Trying to write to a null pointer")
	elseif(_LLVM_CMAKE_STORE_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):(.+)$")
		set(_LLVM_CMAKE_STORE_OFFSET ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_STORE_REF_ENTITY ${CMAKE_MATCH_2})
		list(REMOVE_AT ${_LLVM_CMAKE_STORE_REF_ENTITY} ${_LLVM_CMAKE_STORE_OFFSET})
		list(INSERT ${_LLVM_CMAKE_STORE_REF_ENTITY} ${_LLVM_CMAKE_STORE_OFFSET} ${ARGN})
		set(_LLVM_CMAKE_STORE_MODIFIED_ENTITY ${_LLVM_CMAKE_STORE_REF_ENTITY})
		if(${_LLVM_CMAKE_STORE_PTR} MATCHES "^_LLVM_CMAKE_PTR.*\\.EXT.*$")
			list(APPEND ${_LLVM_CMAKE_STORE_FUNC_MODLIST} ${_LLVM_CMAKE_STORE_REF_ENTITY})
		endif()
	elseif(_LLVM_CMAKE_STORE_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.EXT.*:(.+)$")
		set(_LLVM_CMAKE_STORE_REAL_PTR ${CMAKE_MATCH_1})
		set(${_LLVM_CMAKE_STORE_REAL_PTR} ${ARGN})
		set(_LLVM_CMAKE_STORE_MODIFIED_ENTITY ${_LLVM_CMAKE_STORE_REAL_PTR})
		list(APPEND ${_LLVM_CMAKE_STORE_FUNC_MODLIST} ${_LLVM_CMAKE_STORE_REAL_PTR})
	else()
		set(${_LLVM_CMAKE_STORE_PTR} ${ARGN})
		set(_LLVM_CMAKE_STORE_MODIFIED_ENTITY ${_LLVM_CMAKE_STORE_PTR})
	endif()
	set(${_LLVM_CMAKE_STORE_MODIFIED_ENTITY} ${${_LLVM_CMAKE_STORE_MODIFIED_ENTITY}} PARENT_SCOPE)
	set(${_LLVM_CMAKE_STORE_FUNC_MODLIST} ${${_LLVM_CMAKE_STORE_FUNC_MODLIST}} PARENT_SCOPE)
endfunction()

macro(_LLVM_CMAKE_APPLY_MODIFIED_LIST _LLVM_CMAKE_APPLY_MODIFIED_LIST_FUNC_MODLIST)
	list(REMOVE_DUPLICATES ${_LLVM_CMAKE_APPLY_MODIFIED_LIST_FUNC_MODLIST})
	foreach(_LLVM_CMAKE_APPLY_MODIFIED_LIST_INTERNAL_ENTITY_NAME ${${_LLVM_CMAKE_APPLY_MODIFIED_LIST_FUNC_MODLIST}})
		set(${_LLVM_CMAKE_APPLY_MODIFIED_LIST_INTERNAL_ENTITY_NAME}
			${${_LLVM_CMAKE_APPLY_MODIFIED_LIST_INTERNAL_ENTITY_NAME}} PARENT_SCOPE)
	endforeach()
endmacro()

function(_LLVM_CMAKE_MAKE_EXTERNAL_PTR _LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE _LLVM_CMAKE_MAKE_EXTERNAL_PTR_RESULT_PTR)
	if(_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE MATCHES "_LLVM_CMAKE_PTR.*")
		if(NOT _LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE MATCHES "_LLVM_CMAKE_PTR.*\\.EXT.*")
			string(REGEX REPLACE "_LLVM_CMAKE_PTR([^:]*):" "_LLVM_CMAKE_PTR\\1.EXT:"
				_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE ${_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE})
		endif()
	else()
		set(_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE "_LLVM_CMAKE_PTR.EXT:${_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE}")
	endif()
	set(${_LLVM_CMAKE_MAKE_EXTERNAL_PTR_RESULT_PTR} ${_LLVM_CMAKE_MAKE_EXTERNAL_PTR_VALUE} PARENT_SCOPE)
endfunction()

function(_LLVM_CMAKE_BYTEARRAY_TO_STRING _LLVM_CMAKE_BYTEARRAY_TO_STRING_STR_NAME)
	string(ASCII ${ARGN} ${_LLVM_CMAKE_BYTEARRAY_TO_STRING_STR_NAME})
	set(${_LLVM_CMAKE_BYTEARRAY_TO_STRING_STR_NAME} ${${_LLVM_CMAKE_BYTEARRAY_TO_STRING_STR_NAME}} PARENT_SCOPE)
endfunction()

function(_LLVM_CMAKE_STRING_TO_BYTEARRAY _LLVM_CMAKE_STRING_TO_BYTEARRAY_STR _LLVM_CMAKE_STRING_TO_BYTEARRAY_ARRAY_NAME)
	string(HEX ${_LLVM_CMAKE_STRING_TO_BYTEARRAY_STR} _LLVM_CMAKE_STRING_TO_BYTEARRAY_HEX_STR)
	string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1;" _LLVM_CMAKE_STRING_TO_BYTEARRAY_RESULT_ARRAY "${_LLVM_CMAKE_STRING_TO_BYTEARRAY_HEX_STR}")
	list(POP_BACK _LLVM_CMAKE_STRING_TO_BYTEARRAY_RESULT_ARRAY)
	set(${_LLVM_CMAKE_STRING_TO_BYTEARRAY_ARRAY_NAME} ${_LLVM_CMAKE_STRING_TO_BYTEARRAY_RESULT_ARRAY} PARENT_SCOPE)
endfunction()

function(_LLVM_CMAKE_POINTER_APPLY_OFFSET _LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR _LLVM_CMAKE_POINTER_APPLY_OFFSET_OFFSET _LLVM_CMAKE_POINTER_APPLY_OFFSET_RESULT)
	if(_LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR MATCHES "^_LLVM_CMAKE_PTR(.*\\.GEP.*):([0-9]+):(.+)$")
		set(_LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR_OFFSET ${CMAKE_MATCH_2})
		set(_LLVM_CMAKE_POINTER_APPLY_OFFSET_REF_ENTITY ${CMAKE_MATCH_3})
		math(EXPR _LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR_OFFSET
			"${_LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR_OFFSET} + ${_LLVM_CMAKE_POINTER_APPLY_OFFSET_OFFSET}")
		set(${_LLVM_CMAKE_POINTER_APPLY_OFFSET_RESULT}
			"_LLVM_CMAKE_PTR${CMAKE_MATCH_1}:${_LLVM_CMAKE_POINTER_APPLY_OFFSET_PTR_OFFSET}:${_LLVM_CMAKE_POINTER_APPLY_OFFSET_REF_ENTITY}"
			PARENT_SCOPE)
	else()
		message(FATAL_ERROR "Pointer should be a gep pointer")
	endif()
endfunction()

function(_LLVM_CMAKE_POINTER_OFFSET _LLVM_CMAKE_POINTER_OFFSET_TO _LLVM_CMAKE_POINTER_OFFSET_FROM _LLVM_CMAKE_POINTER_OFFSET_RESULT)
	if(_LLVM_CMAKE_POINTER_OFFSET_TO MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):(.+)$")
		set(_LLVM_CMAKE_POINTER_OFFSET_TO_OFFSET ${CMAKE_MATCH_1})
		set(_LLVM_CMAKE_POINTER_OFFSET_TO_REF_ENTITY ${CMAKE_MATCH_2})
		if(_LLVM_CMAKE_POINTER_OFFSET_FROM MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):(.+)$")
			set(_LLVM_CMAKE_POINTER_OFFSET_FROM_OFFSET ${CMAKE_MATCH_1})
			set(_LLVM_CMAKE_POINTER_OFFSET_FROM_REF_ENTITY ${CMAKE_MATCH_2})

			if(NOT _LLVM_CMAKE_POINTER_OFFSET_TO_REF_ENTITY STREQUAL _LLVM_CMAKE_POINTER_OFFSET_FROM_REF_ENTITY)
				message(FATAL_ERROR "From and to should reference to same object")
			endif()

			math(EXPR ${_LLVM_CMAKE_POINTER_OFFSET_RESULT}
				"${_LLVM_CMAKE_POINTER_OFFSET_TO_OFFSET} - ${_LLVM_CMAKE_POINTER_OFFSET_FROM_OFFSET}")
			set(${_LLVM_CMAKE_POINTER_OFFSET_RESULT} ${${_LLVM_CMAKE_POINTER_OFFSET_RESULT}} PARENT_SCOPE)
			return()
		endif()
	endif()
	message(FATAL_ERROR "From and to should be a gep pointer")
endfunction()

function(_LLVM_CMAKE_EXTRACT_POINTER_OFFSET _LLVM_CMAKE_EXTRACT_POINTER_OFFSET_PTR _LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT)
	if(_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_PTR MATCHES "^_LLVM_CMAKE_PTR.*\\.GEP.*:([0-9]+):.+$")
		set(_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_PTR_OFFSET ${CMAKE_MATCH_1})
		set(${_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT} ${_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_PTR_OFFSET} PARENT_SCOPE)
	else()
		message(FATAL_ERROR "Pointer is not a gep pointer")
	endif()
endfunction()

function(_LLVM_CMAKE_POINTER_TO_INT _LLVM_CMAKE_POINTER_TO_INT_PTR _LLVM_CMAKE_POINTER_TO_INT_PTR_SIZE _LLVM_CMAKE_POINTER_TO_INT_RESULT)
	_LLVM_CMAKE_EXTRACT_POINTER_OFFSET(${_LLVM_CMAKE_POINTER_TO_INT_PTR} _LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT_VALUE)
	math(EXPR _LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT_VALUE "${_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT_VALUE} * ${_LLVM_CMAKE_POINTER_TO_INT_PTR_SIZE}")
	set(${_LLVM_CMAKE_POINTER_TO_INT_RESULT} ${_LLVM_CMAKE_EXTRACT_POINTER_OFFSET_RESULT_VALUE} PARENT_SCOPE)
endfunction()

# End of LLVM CMake intrinsics

)CMakeIntrinsics";

	void NormalizeIdentifier(std::string& identifier)
	{
		std::replace(identifier.begin(), identifier.end(), '$', '_');
		std::replace(identifier.begin(), identifier.end(), '.', '_');
	}

	std::string NormalizeIdentifier(StringRef identifier)
	{
		std::string id{ identifier };
		NormalizeIdentifier(id);
		return id;
	}
} // namespace

char CMakeBackend::ID{};

bool CMakeBackend::doInitialization(Module& M)
{
	m_DataLayout = std::make_unique<DataLayout>(&M);
	m_IntrinsicLowering = std::make_unique<IntrinsicLowering>(*m_DataLayout);

	emitModulePrologue(M);

	return false;
}

bool CMakeBackend::doFinalization(Module& M)
{
	emitModuleEpilogue(M);

	m_IntrinsicLowering.reset();
	m_DataLayout.reset();

	return false;
}

bool CMakeBackend::runOnFunction(Function& F)
{
	m_CurrentFunction = &F;
	const auto& checkNeedGotoPass = getAnalysis<CheckNeedGotoPass>();
	if (checkNeedGotoPass.isCurrentFunctionNeedGoto())
	{
		m_CurrentStateInfo.emplace(checkNeedGotoPass.getCurrentStateInfo());
	}
	else
	{
		m_CurrentStateInfo.reset();
	}
	m_CurrentIntent = 0;
	m_CurrentTemporaryID = 0;
	m_TemporaryID.clear();
	m_CurrentFunctionLocalEntityNames.clear();

	auto modified = lowerIntrinsics(F);
	// 不能依赖 PHIElimination，只好自己实现
	modified |= lowerPHINode(F);
#ifndef NDEBUG
	if (modified)
	{
		outs() << "Function modified: \n" << F << "\n";
	}
#endif

	emitFunction(F);

	return modified;
}

void CMakeBackend::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
	au.addRequired<CheckNeedGotoPass>();
	au.setPreservesCFG();
}

void CMakeBackend::visitInstruction(Instruction& I)
{
	errs() << "Unimplemented: " << I << "\n";

	assert(!"Unimplemented");
	std::terminate();
}

void CMakeBackend::visitCastInst(CastInst& I)
{
	const auto srcType = I.getSrcTy();
	const auto dstType = I.getDestTy();

	const auto src = I.getOperand(0);
	const auto resultName = getValueName(&I);

	// TODO: 当前未实现转换，仅允许值不实际发生变化的情况
	if (srcType == dstType || (srcType->isIntegerTy() && dstType->isIntegerTy()))
	{
		const auto operand = evalOperand(src);
		emitIntent();
		m_Out << "set(" << resultName << " " << operand << ")\n";
	}
	else if (const auto srcPtrType = dyn_cast<PointerType>(srcType))
	{
		if (dstType->isIntegerTy())
		{
			const auto operand = evalOperand(src);
			const auto srcElemType = srcType->getPointerElementType();

			emitIntent();
			m_Out << "_LLVM_CMAKE_POINTER_TO_INT(" << operand << " "
			      << (getTypeSizeInBits(srcElemType) / 8) << " " << resultName << ")\n";
		}
		else if (const auto dstPtrType = dyn_cast<PointerType>(dstType))
		{
			const auto srcElemType = srcPtrType->getElementType();
			const auto dstElemType = dstPtrType->getElementType();

			if (const auto srcElemArrayType = dyn_cast<ArrayType>(srcElemType);
			    srcElemArrayType &&
			    getTypeSizeInBits(srcElemArrayType->getElementType()) == getTypeSizeInBits(dstElemType))
			{
				// 可能是数组退化
				const auto operand = evalOperand(src);
				emitIntent();
				m_Out << "_LLVM_CMAKE_CONSTRUCT_GEP(0 " << operand << " " << resultName << ")\n";
			}
			else
			{
				if (getTypeSizeInBits(srcElemType) != getTypeSizeInBits(dstElemType))
				{
					errs() << I << "\n";
					errs() << "Warning: incompatible pointers casting may be unsafe.\n";
				}

				// 指针 reinterpret_cast
				const auto operand = evalOperand(src);
				emitIntent();
				m_Out << "set(" << resultName << " " << operand << ")\n";
			}
		}
	}
	else
	{
		errs() << I << "\n";
		assert(!"Unimplemented");
		std::terminate();
	}
}

void CMakeBackend::visitReturnInst(ReturnInst& i)
{
	using namespace std::literals::string_literals;

	// CMake 不能返回值，太菜了
	if (i.getNumOperands())
	{
		const auto opName = evalOperand(i.getOperand(0));
		emitIntent();
		m_Out << "set(" << getFunctionReturnValueName(m_CurrentFunction) << " " << opName
		      << " PARENT_SCOPE)\n";
	}

	const auto functionModifiedListName =
	    getFunctionModifiedExternalVariableListName(m_CurrentFunction);

	if (!m_CurrentFunctionLocalEntityNames.empty())
	{
		emitIntent();
		m_Out << "list(REMOVE_ITEM " << functionModifiedListName;
		for (const auto& entityName : m_CurrentFunctionLocalEntityNames)
		{
			m_Out << " " << entityName;
		}
		m_Out << ")\n";
	}

	emitIntent();
	m_Out << "_LLVM_CMAKE_APPLY_MODIFIED_LIST(" << functionModifiedListName << ")\n";
	emitIntent();
	m_Out << "set(" << getFunctionModifiedExternalVariableListName(m_CurrentFunction, true) << " ${"
	      << functionModifiedListName << "} PARENT_SCOPE)\n";

	emitIntent();
	m_Out << "return()\n";
}

void CMakeBackend::visitCallInst(CallInst& I)
{
	if (isa<InlineAsm>(I.getCalledOperand()))
	{
		visitInlineAsm(I);
		return;
	}
	else if (I.getCalledFunction() && I.getCalledFunction()->isIntrinsic())
	{
		visitIntrinsics(I);
		return;
	}

	std::vector<std::string> argumentList;

	for (std::size_t i = 0; i < I.getNumArgOperands(); ++i)
	{
		const auto op = I.getArgOperand(i);
		auto opExpr = evalOperand(op);

		// 若传入参数是指针，标记指针为指向外部的
		if (op->getType()->isPointerTy())
		{
			auto resultName = allocateTemporaryName();

			emitIntent();
			m_Out << "_LLVM_CMAKE_MAKE_EXTERNAL_PTR(" << opExpr << " " << resultName << ")\n";

			argumentList.emplace_back("${" + resultName + "}");
		}
		else
		{
			argumentList.emplace_back(std::move(opExpr));
		}
	}

	const auto callOp = I.getCalledOperand();
	if (!callOp)
	{
		assert(!"Error");
		std::terminate();
	}

	Type* returnType;
	std::string returnValueName;
	std::string returnModifiedListName;

	emitIntent();
	if (const auto func = dyn_cast<Function>(callOp))
	{
		returnType = func->getReturnType();
		returnValueName = getFunctionReturnValueName(func);
		returnModifiedListName = getFunctionModifiedExternalVariableListName(func, true);
		m_Out << NormalizeIdentifier(func->getName()) << "(";
	}
	else
	{
		const auto callOpType = callOp->getType();
		if (const auto ptr = dyn_cast<PointerType>(callOpType))
		{
			const auto ptrValue = evalOperand(callOp);

			const auto funcType = dyn_cast<FunctionType>(ptr->getPointerElementType());
			if (!funcType)
			{
				assert(!"Error");
				std::terminate();
			}

			returnType = funcType->getReturnType();
			returnValueName = "_LLVM_CMAKE_RETURN_VALUE";
			returnModifiedListName = "_LLVM_CMAKE_RETURN_MODIFIED_EXTERNAL_VARIABLE_LIST";
			m_Out << "_LLVM_CMAKE_INVOKE_FUNCTION_PTR(" << ptrValue << " ";
		}
		else
		{
			assert(!"Error");
			std::terminate();
		}
	}

	if (!argumentList.empty())
	{
		m_Out << "" << argumentList.front() << "";
		for (auto iter = std::next(argumentList.begin()); iter != argumentList.end(); ++iter)
		{
			m_Out << " " << *iter;
		}
	}
	m_Out << ")\n";

	if (!returnType->isVoidTy())
	{
		emitIntent();
		m_Out << "set(" << getValueName(&I) << " ${" << returnValueName << "})\n";
	}

	emitIntent();
	m_Out << "list(APPEND " << getFunctionModifiedExternalVariableListName(m_CurrentFunction) << " ${"
	      << returnModifiedListName << "})\n";
}

void CMakeBackend::visitInlineAsm(CallInst& I)
{
	const auto as = cast<InlineAsm>(I.getCalledOperand());
	const auto constraints = as->ParseConstraints();
	const auto retValueName = getValueName(&I);
	std::unordered_map<std::size_t, std::string> outputVars;
	std::unordered_map<std::size_t, std::string> inputVars;
	std::size_t argCount = 0;
	for (std::size_t i = 0; i < constraints.size(); ++i)
	{
		const auto& info = constraints[i];
		if (info.Type == InlineAsm::isOutput)
		{
			outputVars.emplace(i, allocateTemporaryName());
		}
		else if (info.Type == InlineAsm::isInput)
		{
			const auto arg = I.getArgOperand(argCount++);
			inputVars.emplace(i, evalOperand(arg));
		}
	}

	std::string_view templateString = as->getAsmString();
	std::string result;

	while (true)
	{
		const auto nextDollar = templateString.find('$');
		if (nextDollar != std::string_view::npos)
		{
			result.append(templateString.begin(), nextDollar);
			if (nextDollar == templateString.size())
			{
				assert("Invalid asm");
				std::terminate();
			}
			const auto nextChar = templateString[nextDollar + 1];
			if (nextChar == '$')
			{
				result.append(1, '$');
				templateString = templateString.substr(nextDollar + 2);
			}
			else if (nextChar >= '0' && nextChar <= '9')
			{
				unsigned index;
				if (const auto [ptr, ec] = std::from_chars(templateString.begin() + nextDollar + 1,
				                                           templateString.end(), index);
				    ec == std::errc{}) [[likely]]
				{
					if (index >= constraints.size())
					{
						assert(!"Invalid index");
						std::terminate();
					}
					const auto refConstraint = constraints[index];
					if (refConstraint.Type == InlineAsm::isInput)
					{
						result.append(inputVars[index]);
					}
					else if (refConstraint.Type == InlineAsm::isOutput)
					{
						result.append(outputVars[index]);
					}
					else
					{
						assert("Invalid asm");
						std::terminate();
					}
					templateString = templateString.substr(ptr - templateString.begin());
				}
				else
				{
					assert(!"Impossible");
					std::terminate();
				}
			}
			else
			{
				assert("Invalid asm");
				std::terminate();
			}
		}
		else
		{
			result.append(templateString);
			break;
		}
	}

	emitIntent();
	m_Out << result << "\n";

	if (!outputVars.empty())
	{
		emitIntent();
		m_Out << "set(" << retValueName << " \"";
		auto iter = outputVars.begin();
		m_Out << "${" << iter->second << "}";
		++iter;
		for (; iter != outputVars.end(); ++iter)
		{
			m_Out << ";${" << iter->second << "}";
		}
		m_Out << "\")\n";
	}
}

void CMakeBackend::visitBinaryOperator(llvm::BinaryOperator& I)
{
	emitBinaryExpr(I.getOpcode(), I.getOperand(0), I.getOperand(1), getValueName(&I));
}

void CMakeBackend::visitLoadInst(llvm::LoadInst& I)
{
	const auto destName = getValueName(&I);
	emitLoad(destName, I.getOperand(0));
}

void CMakeBackend::visitStoreInst(llvm::StoreInst& I)
{
	emitStore(I.getOperand(0), I.getOperand(1));
}

void CMakeBackend::visitAllocaInst(llvm::AllocaInst& I)
{
	const auto elemType = I.getAllocatedType();
	assert(elemType == I.getType()->getPointerElementType());

	const auto elemSize = I.getArraySize();
	if (const auto constSize = dyn_cast<ConstantInt>(elemSize))
	{
		const auto size = constSize->getValue().getZExtValue();

		auto objectName = allocateTemporaryName();

		emitIntent();
		m_Out << "set(" << objectName << " \"";
		const auto typeLayout = getTypeLayout(elemType);
		for (std::size_t i = 0; i < size; ++i)
		{
			m_Out << typeLayout;
		}
		m_Out << "\")\n";

		// 结果为指针
		emitIntent();
		m_Out << "set(" << getValueName(&I) << " " << objectName << ")\n";

		m_CurrentFunctionLocalEntityNames.emplace_back(std::move(objectName));
	}
	else
	{
		assert(!"Not implemented");
		std::terminate();
	}
}

void CMakeBackend::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
	emitGetElementPtr(I.getPointerOperand(), gep_type_begin(I), gep_type_end(I), getValueName(&I));
}

// TODO: 代码重复
void CMakeBackend::visitExtractValueInst(llvm::ExtractValueInst& I)
{
	// 不是指针
	const auto aggregate = I.getAggregateOperand();
	const auto aggregateValue = evalOperand(aggregate);

	std::size_t realIdx{};
	auto type = aggregate->getType();
	for (const auto idx : I.indices())
	{
		if (const auto arrType = dyn_cast<ArrayType>(type))
		{
			const auto elemType = arrType->getElementType();
			realIdx += getTypeFieldCount(elemType) * idx;
			type = elemType;
		}
		else if (const auto structType = dyn_cast<StructType>(type))
		{
			for (std::size_t i = 0; i < idx; ++i)
			{
				realIdx += getTypeFieldCount(structType->getElementType(i));
			}
			type = structType->getElementType(idx);
		}
		else
		{
			assert(!"Unimplemented");
			std::terminate();
		}
	}

	emitIntent();
	const auto aggregateTmpValue = allocateTemporaryName();
	m_Out << "set(" << aggregateTmpValue << " " << aggregateValue << ")\n";
	emitIntent();
	m_Out << "list(GET " << aggregateTmpValue << " " << realIdx << " " << getValueName(&I) << ")\n";
}

void CMakeBackend::visitInsertValueInst(llvm::InsertValueInst& I)
{
	// 不是指针
	const auto aggregate = I.getAggregateOperand();
	const auto aggregateValue = evalOperand(aggregate);

	const auto value = I.getInsertedValueOperand();
	const auto valueValue = evalOperand(value);

	std::size_t realIdx{};
	auto type = aggregate->getType();
	for (const auto idx : I.indices())
	{
		if (const auto arrType = dyn_cast<ArrayType>(type))
		{
			const auto elemType = arrType->getElementType();
			realIdx += getTypeFieldCount(elemType) * idx;
			type = elemType;
		}
		else if (const auto structType = dyn_cast<StructType>(type))
		{
			for (std::size_t i = 0; i < idx; ++i)
			{
				realIdx += getTypeFieldCount(structType->getElementType(i));
			}
			type = structType->getElementType(idx);
		}
		else
		{
			assert(!"Unimplemented");
			std::terminate();
		}
	}

	const auto valueName = getValueName(&I);
	emitIntent();
	m_Out << "set(" << valueName << " " << aggregateValue << ")\n";
	emitIntent();
	m_Out << "list(REMOVE_AT " << valueName << " " << realIdx << ")\n";
	emitIntent();
	m_Out << "list(INSERT " << valueName << " " << realIdx << " " << valueValue << ")\n";
}

// 简单 if 形式
// bb1 -> bb2 -> ... -> bb3 -> bb4
//     \-------------------/
//
// if-else 形式
// else 接 if 不影响判断
//                            /---------------------\ 
// bb1 -> bb2 -> ... -> bb3 -/  -> bb4 -> ... -> bb5 -> bb6
//     \---------------------- /
void CMakeBackend::visitBranchInst(BranchInst& I)
{
	const auto bb = I.getParent();
	const auto next = bb->getNextNode();

	if (I.isConditional())
	{
		const auto cond = I.getCondition();
		const auto condValue = evalOperand(cond);

		auto trueBranch = I.getSuccessor(0);
		auto falseBranch = I.getSuccessor(1);

		// 有效时所有分支全关联状态，直接跳转
		if (m_CurrentStateInfo)
		{
			auto trueStateId = m_CurrentStateInfo->GetStateID(trueBranch);
			auto falseStateId = m_CurrentStateInfo->GetStateID(falseBranch);
			if (next == trueBranch || next == falseBranch)
			{
				// 下面代码视 false 分支为跳转
				// true 分支直接继续向下执行，因此生成 false 分支跳转
				if (next == trueBranch)
				{
					emitIntent();
					m_Out << "if(NOT " << condValue << ")\n";
				}
				else if (next == falseBranch)
				{
					emitIntent();
					m_Out << "if(" << condValue << ")\n";
					std::swap(trueBranch, falseBranch);
					std::swap(trueStateId, falseStateId);
				}
				else
				{
					// 是谁生成这种代码的？
					return;
				}

				++m_CurrentIntent;
				emitIntent();
				m_Out << "set(_LLVM_CMAKE_LOOP_STATE \"" << falseStateId << "\")\n";
				emitIntent();
				m_Out << "continue()\n";
				--m_CurrentIntent;
				emitIntent();
				m_Out << "endif()\n";

				return;
			}
			assert(trueStateId != -1 && falseStateId != -1);
			emitIntent();
			m_Out << "if(" << condValue << ")\n";
			++m_CurrentIntent;
			emitIntent();
			m_Out << "set(_LLVM_CMAKE_LOOP_STATE \"" << trueStateId << "\")\n";
			--m_CurrentIntent;
			emitIntent();
			m_Out << "else()\n";
			++m_CurrentIntent;
			emitIntent();
			m_Out << "set(_LLVM_CMAKE_LOOP_STATE \"" << falseStateId << "\")\n";
			--m_CurrentIntent;
			emitIntent();
			m_Out << "endif()\n";
			emitIntent();
			m_Out << "continue()\n";
			return;
		}

		if (next != trueBranch)
		{
			if (next == falseBranch)
			{
				emitIntent();
				m_Out << "if(NOT " << condValue << ")\n";
				std::swap(trueBranch, falseBranch);
			}
			else
			{
				errs() << I << "\n";
				assert(!"Goto not supported in CMake.");
				std::terminate();
			}
		}
		else
		{
			emitIntent();
			m_Out << "if(" << condValue << ")\n";
		}

		// TODO: 处理 else if
		if (const auto endIfBr = dyn_cast<BranchInst>(falseBranch->getPrevNode()->getTerminator());
		    endIfBr && endIfBr->getNumSuccessors() == 1)
		{
			++m_CurrentIntent;
			const auto endIfBlock = endIfBr->getSuccessor(0);
			m_CondElseEndifStack.push_back({ falseBranch, endIfBlock });
		}
		else
		{
			errs() << I << "\n";
			assert(!"Goto not supported in CMake.");
			std::terminate();
		}
	}
	else
	{
		const auto successor = I.getSuccessor(0);

		if (successor != next)
		{
			if (m_CondElseEndifStack.empty() || m_CondElseEndifStack.back().second != successor)
			{
				if (m_CurrentStateInfo)
				{
					const auto stateId = m_CurrentStateInfo->GetStateID(successor);
					if (stateId != -1)
					{
						emitIntent();
						m_Out << "set(_LLVM_CMAKE_LOOP_STATE \"" << stateId << "\")\n";
						emitIntent();
						m_Out << "continue()\n";
						return;
					}
				}
				errs() << I << "\n";
				assert(!"Goto not supported in CMake.");
				std::terminate();
			}
		}
	}
}

void CMakeBackend::visitICmpInst(llvm::ICmpInst& I)
{
	const auto resultName = getValueName(&I);

	const auto lhs = I.getOperand(0);
	const auto rhs = I.getOperand(1);
	const auto pred = I.getPredicate();

	const auto lhsOp = evalOperand(lhs);
	const auto rhsOp = evalOperand(rhs);

	emitIntent();
	m_Out << "if(";
	switch (pred)
	{
	case CmpInst::ICMP_EQ:
		m_Out << lhsOp << " EQUAL ";
		break;
	case CmpInst::ICMP_NE:
		m_Out << "NOT " << lhsOp << " EQUAL ";
		break;
	case CmpInst::ICMP_SGE:
	case CmpInst::ICMP_UGE:
		m_Out << lhsOp << " GREATER_EQUAL ";
		break;
	case CmpInst::ICMP_SGT:
	case CmpInst::ICMP_UGT:
		m_Out << lhsOp << " GREATER ";
		break;
	case CmpInst::ICMP_SLE:
	case CmpInst::ICMP_ULE:
		m_Out << lhsOp << " LESS_EQUAL ";
		break;
	case CmpInst::ICMP_SLT:
	case CmpInst::ICMP_ULT:
		m_Out << lhsOp << " LESS ";
		break;
	default:
		assert(!"Impossible");
		std::terminate();
	}
	m_Out << rhsOp << ")\n";

	++m_CurrentIntent;
	emitIntent();
	m_Out << "set(" << resultName << " 1)\n";
	--m_CurrentIntent;

	emitIntent();
	m_Out << "else()\n";

	++m_CurrentIntent;
	emitIntent();
	m_Out << "set(" << resultName << " 0)\n";
	--m_CurrentIntent;

	emitIntent();
	m_Out << "endif()\n";
}

void CMakeBackend::emitModuleInfo(llvm::Module& m)
{
	m_Out << "# This file is generated by LLVMCMakeBackend, do not edit!\n";
	m_Out << "# From module " << m.getModuleIdentifier() << "\n\n";
}

void CMakeBackend::emitModulePrologue(llvm::Module& m)
{
	emitModuleInfo(m);
	emitIntrinsics();
	emitGlobals(m);

	m_Out << "\n";
}

void CMakeBackend::emitModuleEpilogue(llvm::Module& m)
{
	// TODO: 输出类型布局元数据
}

void CMakeBackend::emitGlobals(llvm::Module& m)
{
	for (auto& global : m.globals())
	{
		const auto name = NormalizeIdentifier(global.getName());
		if (!global.hasInitializer())
		{
			emitIntent();
			m_Out << "set(" << name << " \"" << getTypeLayout(global.getValueType()) << "\")\n";
		}
		else
		{
			const auto& [result, inlined] = evalConstant(global.getInitializer(), name);
			if (inlined)
			{
				emitIntent();
				m_Out << "set(" << name << " \"" << result << "\")\n";
			}
		}
	}
}

bool CMakeBackend::lowerIntrinsics(Function& f)
{
	auto lowered = false;

	for (auto& bb : f)
	{
		for (auto iter = bb.begin(), end = bb.end(); iter != end;)
		{
			// TODO: 若 lower 后的 intrinsic 包含 intrinsic 的调用，需重新处理
			if (const auto call = dyn_cast<CallInst>(iter++);
			    call && call->getCalledFunction() && call->getCalledFunction()->isIntrinsic() &&
			    std::find(std::begin(ImplementedIntrinsics), std::end(ImplementedIntrinsics),
			              call->getCalledFunction()->getIntrinsicID()) == std::end(ImplementedIntrinsics))
			{
				lowered = true;
				m_IntrinsicLowering->LowerIntrinsicCall(call);
			}
		}
	}

	return lowered;
}

void CMakeBackend::emitIntrinsics()
{
	m_Out << CMakeIntrinsics;
}

void CMakeBackend::visitIntrinsics(CallInst& call)
{
	const auto func = call.getCalledFunction();
	assert(func && func->isIntrinsic());
	const auto id = func->getIntrinsicID();
	assert(id != Intrinsic::not_intrinsic);

	switch (id)
	{
	case Intrinsic::memcpy: {
		// 假设与 load + store 等价

		const auto dstPtr = call.getArgOperand(0);
		const auto srcPtr = call.getArgOperand(1);
		// 忽略 size

		const auto content = allocateTemporaryName();
		emitLoad(content, srcPtr);
		emitStore("${" + content + "}", dstPtr);

		break;
	}
	case Intrinsic::memset: {
		const auto dstPtr = call.getArgOperand(0);
		[[maybe_unused]] const auto value = call.getArgOperand(1);
		// 忽略 size

		// 假设总为 0
		assert(isa<ConstantInt>(value) && cast<ConstantInt>(value)->getValue().isNullValue());

		const auto dst = evalOperand(dstPtr);
		emitIntent();
		m_Out << "list(TRANSFORM " << dst << " REPLACE \".*\" \"0\")\n";

		break;
	}
	default:
		assert(!"Not implemented.");
		std::terminate();
	}
}

bool CMakeBackend::lowerPHINode(llvm::Function& f)
{
	if (f.empty())
	{
		return false;
	}

	auto modified = false;
	IRBuilder<> builder{ &f.getEntryBlock(), f.getEntryBlock().begin() };
	for (auto& bb : f)
	{
		for (auto iter = bb.begin(); iter != bb.end();)
		{
			if (const auto phiNode = dyn_cast<PHINode>(iter++))
			{
				const auto phiCopy = builder.CreateAlloca(phiNode->getType());
				for (std::size_t i = 0, size = phiNode->getNumIncomingValues(); i < size; ++i)
				{
					const auto incomingValue = phiNode->getIncomingValue(i);
					const auto incomingBlock = phiNode->getIncomingBlock(i);

					IRBuilder<> incomingBuilder{ incomingBlock,
						                           incomingBlock->getTerminator()->getIterator() };
					incomingBuilder.CreateStore(incomingValue, phiCopy);
				}

				IRBuilder<> replaceBuilder{ phiNode->getParent(), phiNode->getIterator() };
				const auto load =
				    replaceBuilder.CreateLoad(phiCopy, NormalizeIdentifier(phiNode->getName()));
				phiNode->replaceAllUsesWith(load);
				phiNode->eraseFromParent();

				modified = true;
			}
		}
	}

	return modified;
}

void CMakeBackend::emitIntent()
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
	return (m_CurrentFunction ? "_LLVM_CMAKE_TEMP_${_LLVM_CMAKE_CURRENT_DEPTH}_"
	                          : "_LLVM_CMAKE_TEMP_GLOBAL_") +
	       std::to_string(id);
}

std::string CMakeBackend::allocateTemporaryName()
{
	return getTemporaryName(allocateTemporaryID());
}

std::string CMakeBackend::getValueName(llvm::Value* v)
{
	if (v->hasName())
	{
		if (dyn_cast<GlobalValue>(v))
		{
			return NormalizeIdentifier(v->getName());
		}

		return "_LLVM_CMAKE_${_LLVM_CMAKE_CURRENT_DEPTH}_" + NormalizeIdentifier(v->getName());
	}

	return getTemporaryName(getTemporaryID(v));
}

std::string CMakeBackend::getFunctionReturnValueName(llvm::Function* f)
{
	return "_LLVM_CMAKE_RETURN_VALUE";
}

std::string CMakeBackend::getFunctionModifiedExternalVariableListName(llvm::Function* f,
                                                                      bool isReturning)
{
	using namespace std::literals::string_literals;
	return "_LLVM_CMAKE"s + (isReturning ? "_RETURN_MODIFIED_EXTERNAL_VARIABLE_LIST"
	                                     : "_MODIFIED_EXTERNAL_VARIABLE_LIST");
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
		else if (type->isVoidTy())
		{
			typeName = "Void";
		}
		else if (const auto intType = dyn_cast<IntegerType>(type))
		{
			switch (intType->getBitWidth())
			{
			case 1:
				typeName = "Bool";
				break;
			case 8:
			case 16:
			case 32:
			case 64:
			case 128:
			default:
				typeName = "Integer";
				break;
			}
		}
		else if (const auto ptrType = dyn_cast<PointerType>(type))
		{
			const auto elemType = ptrType->getElementType();
			typeName = "*";
			typeName += getTypeName(elemType);
		}
		else if (type->isArrayTy())
		{
			typeName = "[";
			typeName += getTypeName(type->getArrayElementType());
			typeName += "; ";
			typeName += std::to_string(type->getArrayNumElements());
			typeName += "]";
		}
		else if (const auto funcType = dyn_cast<FunctionType>(type))
		{
			typeName = "(";
			if (funcType->getNumParams())
			{
				typeName += getTypeName(funcType->getParamType(0));

				for (std::size_t i = 1; i < funcType->getNumParams(); ++i)
				{
					typeName += ", ";
					typeName += getTypeName(funcType->getParamType(i));
				}
			}

			typeName += ") -> ";
			typeName += getTypeName(funcType->getReturnType());
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
			result = 0;
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

unsigned CMakeBackend::getTypeSizeInBits(Type* type)
{
	if (!type->isSized())
	{
		return 0;
	}

	if (const auto size = type->getScalarSizeInBits())
	{
		return size;
	}

	if (type->isPointerTy())
	{
		return m_DataLayout->getPointerSizeInBits();
	}

	return 0;
}

llvm::StringRef CMakeBackend::getTypeZeroInitializer(llvm::Type* type)
{
	auto iter = m_TypeZeroInitializerCache.find(type);
	if (iter == m_TypeZeroInitializerCache.end())
	{
		std::string zeroInitializer;
		if (const auto structType = dyn_cast<StructType>(type))
		{
			const auto numFields = structType->getNumElements();
			for (std::size_t i = 0; i < numFields; ++i)
			{
				zeroInitializer += getTypeZeroInitializer(structType->getElementType(i));
				if (i != numFields - 1)
				{
					zeroInitializer += ';';
				}
			}
		}
		else if (const auto arrayType = dyn_cast<ArrayType>(type))
		{
			const auto size = arrayType->getNumElements();
			auto singleElem = getTypeZeroInitializer(arrayType->getElementType());
			for (std::size_t i = 0; i < size; ++i)
			{
				zeroInitializer += singleElem;
				if (i != size - 1)
				{
					zeroInitializer += ';';
				}
			}
		}
		else if (const auto intType = dyn_cast<IntegerType>(type))
		{
			switch (intType->getBitWidth())
			{
			case 1:
				zeroInitializer = "0";
				break;
			case 8:
				// TODO: 空字符串？目前作为整数类型处理
			default:
				zeroInitializer = "0";
				break;
			}
		}
		else if (isa<PointerType>(type))
		{
			zeroInitializer = "_LLVM_CMAKE_PTR.NUL";
		}
		else
		{
			assert(!"Unimplemented");
			std::terminate();
		}

		std::tie(iter, std::ignore) =
		    m_TypeZeroInitializerCache.emplace(type, std::move(zeroInitializer));
	}

	return iter->second;
}

std::unordered_set<llvm::GlobalValue*> const&
CMakeBackend::getReferencedGlobalValues(llvm::Function& f)
{
	auto iter = m_FunctionReferencedGlobalValues.find(&f);
	if (iter == m_FunctionReferencedGlobalValues.end())
	{
		std::unordered_set<llvm::GlobalValue*> referencedGlobalValues;

		for (auto& bb : f)
		{
			for (auto& ins : bb)
			{
				const auto opNum = ins.getNumOperands();
				for (std::size_t i = 0; i < opNum; ++i)
				{
					const auto op = ins.getOperand(i);
					if (const auto globalValue = dyn_cast<GlobalValue>(op))
					{
						referencedGlobalValues.emplace(globalValue);
					}
				}
			}
		}

		std::tie(iter, std::ignore) =
		    m_FunctionReferencedGlobalValues.emplace(&f, std::move(referencedGlobalValues));
	}

	return iter->second;
}

void CMakeBackend::emitFunction(Function& f)
{
	m_Out << "function(" << NormalizeIdentifier(f.getName()) << ")\n";

	++m_CurrentIntent;
	emitFunctionPrologue(f);
	--m_CurrentIntent;

	if (m_CurrentStateInfo)
	{
		emitStateMachineFunction(f);
	}
	else
	{
		for (auto& bb : f)
		{
			emitBasicBlock(&bb);
		}
	}

	++m_CurrentIntent;
	emitFunctionEpilogue(f);
	--m_CurrentIntent;

	m_Out << "endfunction()\n\n";
}

void CMakeBackend::emitStateMachineFunction(llvm::Function& f)
{
	assert(m_CurrentStateInfo);

	m_CurrentStateInfo->Init();

	++m_CurrentIntent;
	emitIntent();
	m_Out << "set(_LLVM_CMAKE_LOOP_STATE 0)\n";
	emitIntent();
	m_Out << "while(1)\n";

	std::size_t curStateId = 0;
	auto shouldEmitSectionProlog = true;

	for (auto& bb : f)
	{
		if (shouldEmitSectionProlog)
		{
			++m_CurrentIntent;
			emitIntent();
			m_Out << "if(_LLVM_CMAKE_LOOP_STATE STREQUAL \"" << curStateId << "\")\n";
			shouldEmitSectionProlog = false;
		}

		emitBasicBlock(&bb, false);

		const auto terminator = bb.getTerminator();
		const auto br = llvm::dyn_cast<BranchInst>(terminator);
		const auto isFallThrough =
		    br && br->isUnconditional() && br->getSuccessor(0) == bb.getNextNode();
		++m_CurrentIntent;
		visit(terminator);
		--m_CurrentIntent;

		const auto stateId = m_CurrentStateInfo->GetNextStateID();
		if (curStateId != stateId)
		{
			if (isFallThrough)
			{
				++m_CurrentIntent;
				emitIntent();
				m_Out << "set(_LLVM_CMAKE_LOOP_STATE \"" << stateId << "\")\n";
				--m_CurrentIntent;
			}

			emitIntent();
			m_Out << "endif()\n";
			--m_CurrentIntent;
			curStateId = stateId;
			shouldEmitSectionProlog = true;
		}
	}

	emitIntent();
	m_Out << "endwhile()\n";
	--m_CurrentIntent;
}

void CMakeBackend::emitFunctionPrologue(llvm::Function& f)
{
	// 增加深度
	// 不需要恢复，函数返回时会丢弃修改的值
	emitIntent();
	m_Out << "math(EXPR _LLVM_CMAKE_CURRENT_DEPTH \"${_LLVM_CMAKE_CURRENT_DEPTH} + 1\")\n";

	// 输出参数
	std::size_t index{};
	for (auto& arg : f.args())
	{
		const auto argType = arg.getType();
		const auto fieldCount = getTypeFieldCount(argType);

		emitIntent();
		m_Out << "list(SUBLIST ARGN " << index << " " << fieldCount << " " << getValueName(&arg)
		      << ")\n";
		index += fieldCount;
	}
}

void CMakeBackend::emitFunctionEpilogue(llvm::Function& f)
{
}

void CMakeBackend::emitBasicBlock(BasicBlock* bb, bool emitTerminator)
{
#ifndef NDEBUG
	outs() << *bb << "\n";
#endif

	if (!m_CondElseEndifStack.empty())
	{
		if (const auto [elseBlock, endIfBlock] = m_CondElseEndifStack.back();
		    bb == elseBlock && bb != endIfBlock)
		{
			emitIntent();
			m_Out << "else()\n";
		}
		else if (bb == endIfBlock)
		{
			emitIntent();
			m_Out << "endif()\n";
			m_CondElseEndifStack.pop_back();
			--m_CurrentIntent;
		}
	}

	++m_CurrentIntent;
	for (auto iter = bb->begin(), end = std::prev(bb->end()); iter != end; ++iter)
	{
		auto& ins = *iter;
		visit(ins);
	}

	if (emitTerminator)
	{
		visit(bb->getTerminator());
	}
	--m_CurrentIntent;
}

std::string CMakeBackend::evalOperand(llvm::Value* v, llvm::StringRef nameHint)
{
	if (const auto con = dyn_cast<Constant>(v); con)
	{
		return evalConstant(con, nameHint).first;
	}
	else
	{
		// 是变量引用
		return "${" + (nameHint.empty() ? getValueName(v) : static_cast<std::string>(nameHint)) + "}";
	}
}

std::pair<std::string, bool> CMakeBackend::evalConstant(llvm::Constant* con,
                                                        llvm::StringRef nameHint)
{
	// 全局变量在 llvm 中始终以指针引用，故不需解引用
	if (isa<GlobalValue>(con))
	{
		if (isa<Function>(con))
		{
			return { getValueName(con), true };
		}
		return { "_LLVM_CMAKE_PTR.EXT:" + getValueName(con), true };
	}

	auto conName = nameHint.empty() ? getValueName(con) : static_cast<std::string>(nameHint);

	if (const auto ce = dyn_cast<ConstantExpr>(con))
	{
		const auto opCode = ce->getOpcode();
		if (Instruction::isBinaryOp(opCode))
		{
			emitBinaryExpr(static_cast<Instruction::BinaryOps>(opCode), ce->getOperand(0),
			               ce->getOperand(1), conName);
			return { "${" + conName + "}", false };
		}

		if (opCode == Instruction::GetElementPtr)
		{
			emitGetElementPtr(ce->getOperand(0), gep_type_begin(ce), gep_type_end(ce), conName);
			return { "${" + conName + "}", false };
		}

		if (Instruction::isCast(opCode))
		{
			// TODO: 当前 cast 不进行任何操作
			return evalConstant(ce->getOperand(0), conName);
		}

		errs() << "Unimplemented: " << *con << "\n";
		std::terminate();
	}

	if (const auto i = dyn_cast<ConstantInt>(con))
	{
		if (i->getBitWidth() == 1)
		{
			return { i->getValue().isNullValue() ? "0" : "1", true };
		}
		else
		{
			return { i->getValue().toString(10, true), true };
		}
	}
	else if (isa<ConstantPointerNull>(con))
	{
		return { "_LLVM_CMAKE_PTR.NUL", true };
	}
	else if (const auto aggregate = dyn_cast<ConstantAggregate>(con))
	{
		std::vector<std::string> operands;
		for (std::size_t i = 0; i < aggregate->getNumOperands(); ++i)
		{
			const auto op = aggregate->getOperand(i);
			operands.emplace_back(
			    evalConstant(aggregate->getOperand(i), getConstantFieldTempName(conName, i)).first);
		}

		std::string result;
		if (!operands.empty())
		{
			result += operands.front();
			for (auto iter = std::next(operands.begin()); iter != operands.end(); ++iter)
			{
				result += ";";
				result += *iter;
			}
		}

		return { std::move(result), true };
	}
	else if (const auto seq = dyn_cast<ConstantDataSequential>(con))
	{
		std::string result;
		const auto elemType = cast<ArrayType>(seq->getType())->getElementType();
		if (elemType->isIntegerTy())
		{
			if (seq->getNumElements())
			{
				result += cast<ConstantInt>(seq->getElementAsConstant(0))->getValue().toString(10, true);
				for (std::size_t i = 1; i < seq->getNumElements(); ++i)
				{
					result += ";";
					result += cast<ConstantInt>(seq->getElementAsConstant(i))->getValue().toString(10, true);
				}
			}
		}
		else
		{
			assert(!"Unimplemented");
			std::terminate();
		}

		return { std::move(result), true };
	}
	else if (isa<ConstantAggregateZero>(con) || isa<UndefValue>(con))
	{
		return std::pair<std::string, bool>{ getTypeZeroInitializer(con->getType()), true };
	}
	else
	{
		errs() << "Unimplemented: " << *con << "\n";
		std::terminate();
	}

	return { "${" + conName + "}", false };
}

std::string CMakeBackend::getConstantFieldTempName(llvm::StringRef constantName, std::size_t index)
{
	return ("_LLVM_CMAKE_" + constantName + "_FIELD_TEMP_" + std::to_string(index)).str();
}

llvm::StringRef CMakeBackend::getTypeLayout(llvm::Type* type)
{
	auto iter = m_TypeLayoutCache.find(type);
	if (iter == m_TypeLayoutCache.end())
	{
		std::string result;
		if (const auto structType = dyn_cast<StructType>(type))
		{
			if (structType->getNumElements())
			{
				result += getTypeLayout(structType->getElementType(0));
				for (std::size_t i = 1; i < structType->getNumElements(); ++i)
				{
					result += ";";
					result += getTypeLayout(structType->getElementType(i));
				}
			}
		}
		else if (const auto arrayType = dyn_cast<ArrayType>(type))
		{
			if (arrayType->getNumElements())
			{
				const auto elemType = arrayType->getElementType();
				const auto elemTypeLayout = getTypeLayout(elemType);
				result += elemTypeLayout;
				for (std::size_t i = 1; i < arrayType->getNumElements(); ++i)
				{
					result += ";";
					result += elemTypeLayout;
				}
			}
		}
		else
		{
			result = getTypeName(type);
		}

		std::tie(iter, std::ignore) = m_TypeLayoutCache.emplace(type, std::move(result));
	}

	return iter->second;
}

void CMakeBackend::emitLoad(llvm::StringRef resultName, llvm::Value* srcPtr)
{
	const auto operand = evalOperand(srcPtr);
	emitIntent();
	m_Out << "_LLVM_CMAKE_LOAD(" << operand << " " << resultName << ")\n";
}

void CMakeBackend::emitStore(llvm::Value* value, llvm::Value* destPtr)
{
	const auto operandValue = evalOperand(value);
	emitStore(operandValue, destPtr);
}

void CMakeBackend::emitStore(llvm::StringRef valueExpr, llvm::Value* destPtr)
{
	const auto operandDest = evalOperand(destPtr);

	emitIntent();
	m_Out << "_LLVM_CMAKE_STORE(" << getFunctionModifiedExternalVariableListName(m_CurrentFunction)
	      << " " << operandDest << " " << valueExpr << ")\n";
}

void CMakeBackend::emitBinaryExpr(llvm::Instruction::BinaryOps opCode, llvm::Value* lhs,
                                  llvm::Value* rhs, llvm::StringRef name)
{
	if (lhs->getType()->isPointerTy() || rhs->getType()->isPointerTy())
	{
		emitPointerArithmetic(opCode, lhs, rhs, name);
		return;
	}

	const auto operandLhs = evalOperand(lhs);
	const auto operandRhs = evalOperand(rhs);

	emitIntent();
	m_Out << "math(EXPR " << name << " \"" << operandLhs << " ";

	// TODO: 溢出处理
	// NOTE: CMake 的 math 是将所有数作为 int64 处理的
	switch (opCode)
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
	case Instruction::UDiv:
		m_Out << "/";
		break;
	case Instruction::SRem:
	case Instruction::URem:
		m_Out << "%";
		break;
	case Instruction::Or:
		m_Out << "|";
		break;
	case Instruction::And:
		m_Out << "&";
		break;
	case Instruction::Xor:
		m_Out << "^";
		break;
	case Instruction::Shl:
		m_Out << "<<";
		break;
	case Instruction::LShr:
	case Instruction::AShr:
		m_Out << ">>";
		break;
	}

	m_Out << " " << operandRhs << "\")\n";
}

void CMakeBackend::emitPointerArithmetic(llvm::Instruction::BinaryOps opCode, llvm::Value* lhs,
                                         llvm::Value* rhs, llvm::StringRef name)
{
	if (opCode != Instruction::Add && opCode != Instruction::Sub)
	{
		errs() << "Error";
		std::terminate();
	}

	const auto operandLhs = evalOperand(lhs);
	const auto operandRhs = evalOperand(rhs);

	if (lhs->getType()->isPointerTy() && rhs->getType()->isPointerTy())
	{
		if (opCode != Instruction::Sub)
		{
			errs() << "Error";
			std::terminate();
		}

		emitIntent();
		m_Out << "_LLVM_CMAKE_POINTER_OFFSET(" << operandLhs << " " << operandRhs << " " << name
		      << ")\n";
	}
	else
	{
		const auto& [ptrOp, ptrValue, offsetOp, offsetValue] =
		    lhs->getType()->isPointerTy() ? std::tuple{ lhs, operandLhs, rhs, operandRhs }
		                                  : std::tuple{ rhs, operandRhs, lhs, operandLhs };
		if (!offsetOp->getType()->isIntegerTy())
		{
			errs() << "Error type for offset: " << *offsetOp->getType() << "\n";
			std::terminate();
		}

		emitIntent();
		m_Out << "_LLVM_CMAKE_POINTER_APPLY_OFFSET(" << ptrValue
		      << (opCode == Instruction::Add ? " " : " -") << offsetValue << " " << name << ")\n";
	}
}

void CMakeBackend::emitGetElementPtr(llvm::Value* ptrOperand, llvm::gep_type_iterator gepBegin,
                                     llvm::gep_type_iterator gepEnd, llvm::StringRef name)
{
	if (gepBegin == gepEnd)
	{
		assert(!"Not implemented");
		std::terminate();
	}

	std::size_t realIdx{};
	// 系数及对应的表达式
	std::vector<std::pair<std::size_t, std::string>> offsets;

	const auto ptrValue = [&] {
		const auto firstIdx = (gepBegin++).getOperand();
		if (const auto constIdx = dyn_cast<ConstantInt>(firstIdx);
		    !constIdx || !constIdx->getValue().isNullValue())
		{
			const auto resultPtrName = allocateTemporaryName();
			emitPointerArithmetic(Instruction::Add, ptrOperand, firstIdx, resultPtrName);
			return "${" + resultPtrName + "}";
		}
		else
		{
			return evalOperand(ptrOperand);
		}
	}();

	const auto ptrType = cast<PointerType>(ptrOperand->getType());
	auto pointeeType = ptrType->getElementType();
	for (auto iter = gepBegin; iter != gepEnd; ++iter)
	{
		const auto idxOperand = iter.getOperand();
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
			else if (const auto ptrType = dyn_cast<PointerType>(pointeeType))
			{
				// TODO: 确认正确性
				const auto fieldSize = getTypeFieldCount(ptrType->getElementType());
				realIdx += idxValue * fieldSize;
				pointeeType = ptrType->getElementType();
			}
			else
			{
				assert(!"Unsupported type");
				std::terminate();
			}
		}
		else
		{
			// TODO: 当前仅支持数组及指针的动态索引，是否需要支持结构体的动态索引？
			if (const auto arrayType = dyn_cast<ArrayType>(pointeeType))
			{
				offsets.emplace_back(getTypeFieldCount(arrayType->getElementType()),
				                     evalOperand(idxOperand));
			}
			else if (const auto ptrType = dyn_cast<PointerType>(pointeeType))
			{
				offsets.emplace_back(getTypeFieldCount(ptrType->getElementType()), evalOperand(idxOperand));
			}
			else
			{
				assert(!"Not implemented");
				std::terminate();
			}
		}
	}

	// 结果是指针

	const auto offsetName = allocateTemporaryName();

	if (offsets.empty())
	{
		emitIntent();
		m_Out << "set(" << offsetName << " \"" << realIdx << "\")\n";
	}
	else
	{
		emitIntent();
		m_Out << "math(EXPR " << offsetName << " \"" << realIdx;
		for (const auto& [size, expr] : offsets)
		{
			m_Out << " + " << size << " * " << expr;
		}
		m_Out << "\")\n";
	}

	emitIntent();
	m_Out << "_LLVM_CMAKE_CONSTRUCT_GEP(${" << offsetName << "} " << ptrValue << " " << name << ")\n";
}
