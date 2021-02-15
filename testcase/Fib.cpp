extern "C" unsigned long long fib(int count)
{
    if (count < 2)
    {
        return 1;
    }

    unsigned long long result[2]{ 1, 1 };
    for (auto i = 0; i < count - 2; ++i)
    {
        const auto a = i % 2;
        result[a] += result[1 - a];
    }
    return result[0] + result[1];
}
