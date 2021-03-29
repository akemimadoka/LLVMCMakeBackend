#include <cstddef>

std::size_t HeapCount{};

void* operator new(std::size_t size)
{
	const auto id = HeapCount++;
	void* result;
	asm(
		"list(APPEND _LLVM_CMAKE_MODIFIED_EXTERNAL_VARIABLE_LIST _AllocatedObject_%1)\n"
		"string(REPEAT \";0\" %2 _AllocatedObject_%1)\n"
		"set(_AllocatedObject_%1 \"0$%{_AllocatedObject_%1%}\")\n"
		"set(%0 _AllocatedObject_%1)\n"
		: "=r"(result)
		: "r"(id), "r"(size - 1)
		:
	);
	return result;
}

void* operator new[](std::size_t size)
{
	return ::operator new(size);
}

void operator delete(void* ptr) noexcept
{
}

void operator delete[](void* ptr) noexcept
{
}

extern "C"
{
	class CMakeString
	{
	public:
		CMakeString(const char* str)
		{
			asm(
				"_LLVM_CMAKE_EXTRACT_REF_LIST(%1 _CMakeString_TmpList)\n"
				"list(POP_BACK _CMakeString_TmpList)\n"
				"_LLVM_CMAKE_BYTEARRAY_TO_STRING(%0 $%{_CMakeString_TmpList%})\n"
				: "=r"(m_Dummy)
				: "r"(str)
				:
			);
		}

	private:
		char m_Dummy;
	};

	void Print(CMakeString const& str)
	{
		asm(
			"message(%0)\n"
			:
			: "r"(str)
			:
		);
	}

	std::size_t strlen(const char* ptr)
	{
		auto end = ptr;
		while (*end++);
		return end - ptr - 1;
	}

	int* Ptr = nullptr;
	int Array[] = { 1, 2, 3, 4 };

	int Load(int* ptr)
	{
		return *ptr;
	}

	int* GetElem(int idx)
	{
		return Array + idx;
	}

	int LoadOffset(int* ptr, int offset)
	{
		return ptr[offset];
	}

	int GetOffset(int* from, int* to)
	{
		return to - from;
	}

	struct Pair
	{
		int A;
		int B;
	};

	Pair PairArray[5]{};

	Pair CreatePair(int a, int b)
	{
		return { a, b };
	}

	void CopyPair(Pair& dst, Pair const& src)
	{
		dst = src;
	}

	Pair const& GetPair(int i)
	{
		return PairArray[i];
	}

	int GetA(Pair p)
	{
		return p.A;
	}

	void TestInner2(Pair& r)
	{
		r.A = 2;
	}

	void TestInner1(Pair& r)
	{
		TestInner2(r);
	}

	int TestInner0()
	{
		auto pair = CreatePair(0, 1);
		TestInner1(pair);
		return pair.A;
	}

	struct BigObj
	{
		int Payload[100];
	};

	int GetFirst(BigObj b)
	{
		return b.Payload[0];
	}

	BigObj CreateBigObj()
	{
		return {};
	}

	int Func1(int arg)
	{
		return arg + 1;
	}

	int Func2(int arg)
	{
		return arg + 2;
	}

	int (*GetCallback(bool func1))(int)
	{
		return func1 ? Func1 : Func2;
	}

	int InvokeFunc(int (*callback)(int), int arg)
	{
		return callback(arg);
	}

	int AsmTest(int arg1, int arg2)
	{
		int result1, result2;
		asm(
			R"(
message(STATUS "arg1 is %2, arg2 is %3")
math(EXPR %0 "%2 + %3")
math(EXPR %1 "%2 - %3")
)"
			: "=r"(result1), "=r"(result2)
			: "r"(arg1), "r"(arg2)
			:
		);
		return result1 * result2;
	}

	int BranchTest(int cond)
	{
		if (cond == 1)
		{
			return 1;
		}
		else if (cond == 2)
		{
			return 2;
		}
		else
		{
			return 0;
		}
	}

	int LoopTest(int count)
	{
		int result{};
		for (auto i = 0; i < count; ++i)
		{
			if (i > 2)
			{
				result += count;
			}
		}

		return result;
	}

	int LoopTest2(int count)
	{
		int result{};
		do
		{
			result += count;
		} while (--count);
		return result;
	}

	int LoopTest3(int count)
	{
		int result{};
		while (true)
		{
			result += count;
			if (result > 10)
			{
				break;
			}
			result += 1;
		}
		return result;
	}

	int LoopTest4(int count)
	{
		int result{};
		while (true)
		{
			result += count;
			if (result < 10)
			{
				continue;
			}
			else
			{
				break;
			}
			result += 1;
		}
		return result;
	}

	int* TestNew()
	{
		return new int(123);
	}

	int* TestNewArray()
	{
		return new int[2]{ 123, 456 };
	}

	Pair* TestNew2()
	{
		return new Pair{ 123, 456 };
	}

	void TestCMakeString()
	{
		CMakeString str{ "This is a CMakeString" };
		Print(str);
	}
}
