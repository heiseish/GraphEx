#include <benchmark/benchmark.h>
#include <algorithm>

#ifdef USE_BOOST_LOCKLESS_Q
#include "cptl.hpp"
#else
#include "cptl_stl.hpp"
#endif

#include "graphex.hpp"

using namespace GE;

std::function<void(void)> firstFunc = []() -> void {};
std::function<int(void)> secondFunc = []() -> int { return 1; };
std::function<int(int)> thirdFunc = [](int a) -> int { return a + 2; };
std::function<int(int)> fourthFunc = [](int a) -> int { return a * 2; };
std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
    return a % b;
};

static void BM_GraphEX(benchmark::State& state)
{
    for (auto _ : state) {
        GraphEx executor;

        decltype(auto) first = executor.makeNode(firstFunc);
        decltype(auto) second = executor.makeNode(secondFunc);
        decltype(auto) third = executor.makeNode(thirdFunc);
        decltype(auto) fourth = executor.makeNode(fourthFunc);
        decltype(auto) fifth = executor.makeNode(fifthFunc);

        second->setParent(first);
        third->setParent<0>(second);
        fourth->setParent<0>(second);
        fifth->setParent<0>(third);
        fifth->setParent<1>(fourth);

        executor.execute();
    }
}
// Register the function as a benchmark
BENCHMARK(BM_GraphEX);

// Define another benchmark
static void BM_FunctionCall(benchmark::State& state)
{
    for (auto _ : state) {
        firstFunc();
        auto res = secondFunc();
        auto a = thirdFunc(res);
        auto b = thirdFunc(res);
        fifthFunc(a, b);
    }
}
BENCHMARK(BM_FunctionCall);

constexpr int loop_n = 1'000'000;
std::function<void(void)> firstCostlyFunc = []() -> void {
    for (int i = 0; i < loop_n; ++i)
        ;
};
std::function<int(void)> secondCostlyFunc = []() -> int {
    int k = 1;
    for (int i = 0; i < loop_n; ++i)
        k ^= i;
    return k;
};
std::function<int(int)> thirdCostlyFunc = [](int a) -> int {
    for (int i = loop_n; i >= 0; --i) {
        if (i & 1)
            a = std::min(a ^ i, i + 10);
    }
    return a;
};
std::function<int(int)> fourthCostlyFunc = [](int a) -> int {
    for (int i = 1'000; i >= 0; --i) {
        for (int j = 1; j <= 1'000; ++j) {
            a ^= (i % j);
            ++a;
        }
    }
    return a;
};
std::function<int(int, int)> fifthCostlyFunc = [](int a, int b) -> int {
    int ret = 1;
    constexpr int MOD = 1e9 + 7;
    b = std::abs(b);
    while (b) {
        if (b & 1)
            ret = (long long)ret * a % MOD;
        a = (long long)a * a % MOD;
        b >>= 1;
    }
    return ret;
};

auto sixCostlyFunc(int a, int b, int c, int d) -> int
{
    a = std::max(a, c);
    b = std::max(b, d);

    int ret = 1;
    constexpr int MOD = 1e9 + 7;
    b = std::abs(b);
    while (b) {
        if (b & 1)
            ret = (long long)ret * a % MOD;
        a = (long long)a * a % MOD;
        b >>= 1;
    }
    return ret;
}

static void BM_GraphEX_Expensive(benchmark::State& state)
{
    for (auto _ : state) {
        GraphEx executor;

        decltype(auto) first = executor.makeNode(firstCostlyFunc);
        decltype(auto) second = executor.makeNode(secondCostlyFunc);
        decltype(auto) third = executor.makeNode(thirdCostlyFunc);
        decltype(auto) fourth = executor.makeNode(fourthCostlyFunc);
        decltype(auto) fifth = executor.makeNode(fifthCostlyFunc);

        second->setParent(first);
        third->setParent<0>(second);
        fourth->setParent<0>(second);
        fifth->setParent<0>(third);
        fifth->setParent<1>(fourth);

        executor.execute();
    }
}
BENCHMARK(BM_GraphEX_Expensive);

static void BM_FunctionCall_Expensive(benchmark::State& state)
{
    for (auto _ : state) {
        firstCostlyFunc();
        auto res = secondCostlyFunc();
        auto a = thirdCostlyFunc(res);
        auto b = fourthCostlyFunc(res);
        fifthCostlyFunc(a, b);
    }
}
BENCHMARK(BM_FunctionCall_Expensive);

static void BM_GraphEX_Expensive_Parallel(benchmark::State& state)
{
    GraphEx executor(4);

    decltype(auto) first = executor.makeNode(secondCostlyFunc);
    decltype(auto) second = executor.makeNode(thirdCostlyFunc);
    decltype(auto) third = executor.makeNode(thirdCostlyFunc);
    decltype(auto) fourth = executor.makeNode(fourthCostlyFunc);
    decltype(auto) fifth = executor.makeNode(fourthCostlyFunc);
    std::function<int(int, int, int, int)> ss = sixCostlyFunc;
    decltype(auto) sixth = executor.makeNode(ss);

    second->setParent<0>(first);
    third->setParent<0>(first);
    fourth->setParent<0>(first);
    fifth->setParent<0>(first);

    sixth->setParent<0>(second);
    sixth->setParent<1>(third);
    sixth->setParent<2>(fourth);
    sixth->setParent<3>(fifth);

    for (auto _ : state) {
        executor.execute();
        executor.reset();
    }
}
BENCHMARK(BM_GraphEX_Expensive_Parallel);

static void BM_FunctionCall_Expensive_NonParallel(benchmark::State& state)
{
    for (auto _ : state) {
        auto res1 = secondCostlyFunc();
        auto res2 = thirdCostlyFunc(res1);
        auto res3 = thirdCostlyFunc(res1);
        auto res4 = fourthCostlyFunc(res1);
        auto res5 = fourthCostlyFunc(res1);
        sixCostlyFunc(res2, res3, res4, res5);
    }
}
BENCHMARK(BM_FunctionCall_Expensive_NonParallel);

static void BM_FunctionCall_Expensive_Parallel(benchmark::State& state)
{
    ctpl::thread_pool pool(4);
    for (auto _ : state) {
        auto res = secondCostlyFunc();
        auto f1 = pool.push(std::bind(thirdCostlyFunc, res));
        auto f2 = pool.push(std::bind(thirdCostlyFunc, res));
        auto f3 = pool.push(std::bind(fourthCostlyFunc, res));
        auto f4 = pool.push(std::bind(fourthCostlyFunc, res));
        sixCostlyFunc(f1.get(), f2.get(), f3.get(), f4.get());
    }
}
BENCHMARK(BM_FunctionCall_Expensive_Parallel);

BENCHMARK_MAIN();