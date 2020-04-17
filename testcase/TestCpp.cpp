extern "C"
{
	int Load(int* ptr)
	{
		return *ptr;
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
}
