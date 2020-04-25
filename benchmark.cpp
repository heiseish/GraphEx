#include <benchmark/benchmark.h>
#include <algorithm>
#include "graphex.hpp"

using namespace GE;

std::function<void(void)> first_func = []() -> void {};
std::function<int(void)> second_func = []() -> int { return 1; };
std::function<int(int)> third_func = [](int a) -> int { return a + 2; };
std::function<int(int)> fourth_func = [](int a) -> int { return a * 2; };
std::function<int(int, int)> fifth_func = [](int a, int b) -> int {
    return a % b;
};

static void BM_GraphEX(benchmark::State& state)
{
    for (auto _ : state) {
        decltype(auto) first = make_node(first_func);
        decltype(auto) second = make_node(second_func);
        decltype(auto) third = make_node(third_func);
        decltype(auto) fourth = make_node(fourth_func);
        decltype(auto) fifth = make_node(fifth_func);

        second.set_parent(first);
        third.set_parent<0>(second);
        fourth.set_parent<0>(second);
        fifth.set_parent<0>(third);
        fifth.set_parent<1>(fourth);
        GraphEx executor;
        executor.register_input_node(&first);
        executor.execute();
    }
}
// Register the function as a benchmark
BENCHMARK(BM_GraphEX);

// Define another benchmark
static void BM_FunctionCall(benchmark::State& state)
{
    for (auto _ : state) {
        first_func();
        auto res = second_func();
        auto a = third_func(res);
        auto b = third_func(res);
        fifth_func(a, b);
    }
}
BENCHMARK(BM_FunctionCall);

constexpr int loop_n = 10'000;
std::function<void(void)> first_costly_func = []() -> void {
    for (int i = 0; i < loop_n; ++i)
        ;
};
std::function<int(void)> second_costly_func = []() -> int {
    int k = 1;
    for (int i = 0; i < loop_n; ++i) k ^= i;
    return k;
};
std::function<int(int)> third_costly_func = [](int a) -> int {
    for (int i = loop_n; i >= 0; --i) {
        if (i & 1) a = std::min(a ^ i, i + 10);
    }
    return a;
};
std::function<int(int)> fourth_costly_func = [](int a) -> int {
    for (int i = 100; i >= 0; --i) {
        for (int j = 1; j <= 100; ++j) {
            a ^= (i % j);
            ++a;
        }
    }
    return a;
};
std::function<int(int, int)> fifth_costly_func = [](int a, int b) -> int {
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

auto six_costly_func(int a, int b, int c, int d) -> int
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
        decltype(auto) first = make_node(first_costly_func);
        decltype(auto) second = make_node(second_costly_func);
        decltype(auto) third = make_node(third_costly_func);
        decltype(auto) fourth = make_node(fourth_costly_func);
        decltype(auto) fifth = make_node(fifth_costly_func);

        second.set_parent(first);
        third.set_parent<0>(second);
        fourth.set_parent<0>(second);
        fifth.set_parent<0>(third);
        fifth.set_parent<1>(fourth);
        GraphEx executor;
        executor.register_input_node(&first);
        executor.execute();
    }
}
BENCHMARK(BM_GraphEX_Expensive);

static void BM_FunctionCall_Expensive(benchmark::State& state)
{
    for (auto _ : state) {
        first_costly_func();
        auto res = second_costly_func();
        auto a = third_costly_func(res);
        auto b = fourth_costly_func(res);
        fifth_costly_func(a, b);
    }
}
BENCHMARK(BM_FunctionCall_Expensive);

static void BM_GraphEX_Expensive_Parallel(benchmark::State& state)
{
    decltype(auto) first = make_node(second_costly_func);
    decltype(auto) second = make_node(third_costly_func);
    decltype(auto) third = make_node(third_costly_func);
    decltype(auto) fourth = make_node(fourth_costly_func);
    decltype(auto) fifth = make_node(fourth_costly_func);
    std::function<int(int, int, int, int)> ss = six_costly_func;
    decltype(auto) sixth = make_node(ss);

    second.set_parent<0>(first);
    third.set_parent<0>(first);
    fourth.set_parent<0>(first);
    fifth.set_parent<0>(first);

    sixth.set_parent<0>(second);
    sixth.set_parent<1>(third);
    sixth.set_parent<2>(fourth);
    sixth.set_parent<3>(fifth);

    GraphEx executor(4);
    executor.register_input_node(&first);
    for (auto _ : state) {
        executor.execute();
        executor.reset();
    }
}
BENCHMARK(BM_GraphEX_Expensive_Parallel);

static void BM_FunctionCall_Expensive_NonParallel(benchmark::State& state)
{
    for (auto _ : state) {
        auto res1 = second_costly_func();
        auto res2 = third_costly_func(res1);
        auto res3 = third_costly_func(res1);
        auto res4 = fourth_costly_func(res1);
        auto res5 = fourth_costly_func(res1);
        six_costly_func(res2, res3, res4, res5);
    }
}
BENCHMARK(BM_FunctionCall_Expensive_NonParallel);

static void BM_FunctionCall_Expensive_Parallel(benchmark::State& state)
{
    for (auto _ : state) {
        auto res1 = second_costly_func();

        // future from a packaged_task
        std::packaged_task<int(int)> task1(third_costly_func);
        std::future<int> f1 = task1.get_future();
        std::thread t1(std::move(task1), res1);

        std::packaged_task<int(int)> task2(third_costly_func);
        std::future<int> f2 = task2.get_future();
        std::thread t2(std::move(task2), res1);

        std::packaged_task<int(int)> task3(fourth_costly_func);
        std::future<int> f3 = task3.get_future();
        std::thread t3(std::move(task3), res1);

        std::packaged_task<int(int)> task4(fourth_costly_func);
        std::future<int> f4 = task4.get_future();
        std::thread t4(std::move(task4), res1);

        // f1.wait();
        // f2.wait();
        // f3.wait();
        // f4.wait();
        six_costly_func(f1.get(), f2.get(), f3.get(), f4.get());

        t1.join();
        t2.join();
        t3.join();
        t4.join();
    }
}
BENCHMARK(BM_FunctionCall_Expensive_Parallel);

BENCHMARK_MAIN();