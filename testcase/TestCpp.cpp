extern "C"
{
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
}
