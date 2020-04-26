#include <benchmark/benchmark.h>
#include <algorithm>
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

        decltype(auto) first = makeNode(executor, firstFunc);
        decltype(auto) second = makeNode(executor, secondFunc);
        decltype(auto) third = makeNode(executor, thirdFunc);
        decltype(auto) fourth = makeNode(executor, fourthFunc);
        decltype(auto) fifth = makeNode(executor, fifthFunc);

        second.setParent(first);
        third.setParent<0>(second);
        fourth.setParent<0>(second);
        fifth.setParent<0>(third);
        fifth.setParent<1>(fourth);

        executor.registerInputNode(&first);
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

constexpr int loop_n = 10'000;
std::function<void(void)> firstCostlyFunc = []() -> void {
    for (int i = 0; i < loop_n; ++i)
        ;
};
std::function<int(void)> secondCostlyFunc = []() -> int {
    int k = 1;
    for (int i = 0; i < loop_n; ++i) k ^= i;
    return k;
};
std::function<int(int)> thirdCostlyFunc = [](int a) -> int {
    for (int i = loop_n; i >= 0; --i) {
        if (i & 1) a = std::min(a ^ i, i + 10);
    }
    return a;
};
std::function<int(int)> fourthCostlyFunc = [](int a) -> int {
    for (int i = 100; i >= 0; --i) {
        for (int j = 1; j <= 100; ++j) {
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
        if (b & 1) ret = (long long)ret * a % MOD;
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
        if (b & 1) ret = (long long)ret * a % MOD;
        a = (long long)a * a % MOD;
        b >>= 1;
    }
    return ret;
}

static void BM_GraphEX_Expensive(benchmark::State& state)
{
    for (auto _ : state) {
        GraphEx executor;

        decltype(auto) first = makeNode(executor, firstCostlyFunc);
        decltype(auto) second = makeNode(executor, secondCostlyFunc);
        decltype(auto) third = makeNode(executor, thirdCostlyFunc);
        decltype(auto) fourth = makeNode(executor, fourthCostlyFunc);
        decltype(auto) fifth = makeNode(executor, fifthCostlyFunc);

        second.setParent(first);
        third.setParent<0>(second);
        fourth.setParent<0>(second);
        fifth.setParent<0>(third);
        fifth.setParent<1>(fourth);

        executor.registerInputNode(&first);
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

    decltype(auto) first = makeNode(executor, secondCostlyFunc);
    decltype(auto) second = makeNode(executor, thirdCostlyFunc);
    decltype(auto) third = makeNode(executor, thirdCostlyFunc);
    decltype(auto) fourth = makeNode(executor, fourthCostlyFunc);
    decltype(auto) fifth = makeNode(executor, fourthCostlyFunc);
    std::function<int(int, int, int, int)> ss = sixCostlyFunc;
    decltype(auto) sixth = makeNode(executor, ss);

    second.setParent<0>(first);
    third.setParent<0>(first);
    fourth.setParent<0>(first);
    fifth.setParent<0>(first);

    sixth.setParent<0>(second);
    sixth.setParent<1>(third);
    sixth.setParent<2>(fourth);
    sixth.setParent<3>(fifth);

    executor.registerInputNode(&first);
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
    for (auto _ : state) {
        auto res1 = secondCostlyFunc();

        // future from a packaged_task
        std::packaged_task<int(int)> task1(thirdCostlyFunc);
        std::future<int> f1 = task1.get_future();
        std::thread t1(std::move(task1), res1);

        std::packaged_task<int(int)> task2(thirdCostlyFunc);
        std::future<int> f2 = task2.get_future();
        std::thread t2(std::move(task2), res1);

        std::packaged_task<int(int)> task3(fourthCostlyFunc);
        std::future<int> f3 = task3.get_future();
        std::thread t3(std::move(task3), res1);

        std::packaged_task<int(int)> task4(fourthCostlyFunc);
        std::future<int> f4 = task4.get_future();
        std::thread t4(std::move(task4), res1);

        // f1.wait();
        // f2.wait();
        // f3.wait();
        // f4.wait();
        sixCostlyFunc(f1.get(), f2.get(), f3.get(), f4.get());

        t1.join();
        t2.join();
        t3.join();
        t4.join();
    }
}
BENCHMARK(BM_FunctionCall_Expensive_Parallel);

BENCHMARK_MAIN();