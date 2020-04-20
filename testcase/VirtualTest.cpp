extern "C"
{
	struct VirtualBase
	{
		virtual ~VirtualBase() = default;

		virtual int Func(int arg)
		{
			return arg + 1;
		}
	};

	struct Derived : VirtualBase
	{
		int Func(int arg) override
		{
			return arg + 2;
		}
	};

	VirtualBase CreateVirtualBase()
	{
		return {};
	}

	Derived CreateDerived()
	{
		return {};
	}

	int InvokeFunc(VirtualBase& obj, int arg)
	{
		return obj.Func(arg);
	}
}
