#include "graphex.hpp"
#include "gtest/gtest.h"

using namespace GE;

class GraphExTest : public ::testing::Test {
};

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph)
{
    GraphEx executor;

    decltype(auto) first =
        executor.makeNode([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        executor.makeNode([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        executor.makeNode([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        executor.makeNode([]() -> void { std::cout << "Running fourth\n"; });
    second->setParent(first);
    fourth->setParent(first);
    second->setParent(third);
    fourth->setParent(third);
    third->setParent(first);

    EXPECT_FALSE(executor.hasCycle());
    executor.execute();

    /**
    Running first
    Running third
    Running second
    Running fourth
    */
}

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph2)
{
    GraphEx executor;

    std::function<int(void)> firstFunc = []() -> int { return 1; };
    decltype(auto) first = executor.makeNode(firstFunc);
    decltype(auto) second =
        executor.makeNode([]() -> void { std::cout << "Running second\n"; });
    second->setParent(first);

    EXPECT_FALSE(executor.hasCycle());
    executor.execute();
    /**
    Running first
    Running third
    Running second
    Running fourth
    */
}

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleGraphWithArgumentPassing)
{
    GraphEx executor;

    decltype(auto) first =
        executor.makeNode([]() -> void { std::cout << "Running first\n"; });

    // Need to explicitly set this one or compiler wont be able to deduce
    // whether this is makeNode<void, Args..> or makeNode<ReturnType, ...>
    std::function<int(void)> secondFunc = []() -> int {
        std::cout << "Running second\nReturn 1\n";
        return 1;
    };
    decltype(auto) second = executor.makeNode(secondFunc);

    // Need to explicitly create std::function, seems like
    // template is unable to deduce the type of lambda
    std::function<int(int)> thirdFunc = [](int a) -> int {
        std::cout << "Running third\nAdding 2: a + 2 == " << a + 2 << "\n";
        return a + 2;
    };
    decltype(auto) third = executor.makeNode(thirdFunc);

    // same as above
    std::function<int(int)> fourthFunc = [](int a) -> int {
        std::cout << "Running fourth\nMultiplying by 2: a * 2 == " << a * 2
                  << "\n";
        return a * 2;
    };
    decltype(auto) fourth = executor.makeNode(fourthFunc);

    std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
        std::cout << "Running fifth\nModding the two numbers: a % b == "
                  << a % b << "\n";
        return a % b;
    };
    decltype(auto) fifth = executor.makeNode(fifthFunc);

    second->setParent(first);
    third->setParent<0>(second);
    fourth->setParent<0>(second);
    fifth->setParent<0>(third);
    fifth->setParent<1>(fourth);

    EXPECT_FALSE(executor.hasCycle());
    executor.execute();
    EXPECT_EQ(third->collect(), 3);
    EXPECT_EQ(fourth->collect(), 2);
    EXPECT_EQ(fifth->collect(), 1);
    /**
    Running first
    Running second
    Return 1
    Running third
    Adding 2: a + 2 == 3
    Running fourth
    Multiplying by 2: a * 2 == 2
    Running fifth
    Modding the two numbers: a % b == 1
    */
}

TEST_F(GraphExTest, CheckGraphHasCycle)
{
    GraphEx executor;

    decltype(auto) first =
        executor.makeNode([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        executor.makeNode([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        executor.makeNode([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        executor.makeNode([]() -> void { std::cout << "Running fourth\n"; });
    second->setParent(first);
    third->setParent(second);
    fourth->setParent(third);
    first->setParent(fourth);

    EXPECT_TRUE(executor.hasCycle());
}

TEST_F(GraphExTest, ShouldBeAbleToHandleMovableObjectCorrectly)
{
    struct MyMoveable {
        int i = 1;
        std::string rand_str = "hello universe";
    };

    {
        GraphEx executor;

        decltype(auto) preprocess = executor.makeNode(
            []() -> void { std::cout << "Running preprocessing\n"; });

        std::function<MyMoveable()> firstFunc = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = executor.makeNode(firstFunc);
        std::function<MyMoveable(MyMoveable)> secondFunc =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "oh shit";
            return a;
        };
        decltype(auto) second = executor.makeNode(secondFunc);

        second->setParent(preprocess);
        second->setParent<0>(first);

        executor.execute();
        auto initial_input = first->collect();
    }

    {
        GraphEx executor;

        decltype(auto) preprocess = executor.makeNode(
            []() -> void { std::cout << "Running preprocessing\n"; });
        std::function<MyMoveable()> firstFunc = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = executor.makeNode(firstFunc);
        std::function<MyMoveable(MyMoveable)> secondFunc =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "just updated";
            return a;
        };
        decltype(auto) second = executor.makeNode(secondFunc);

        second->setParent(preprocess);
        second->setParent<0>(first);

        executor.execute();
        auto final_output = second->collect();
        EXPECT_EQ(final_output.rand_str, "just updated");
        auto initial_input = first->collect();
        EXPECT_EQ(initial_input.rand_str, "hello universe");
    }
}

TEST_F(GraphExTest, ShouldBeAbleToHandleNonCopyableStruct)
{
    GraphEx executor;

    using NonCopyableType = std::unique_ptr<int>;
    std::function<NonCopyableType()> firstFunc = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = executor.makeNode(firstFunc);
    std::function<NonCopyableType(NonCopyableType)> secondFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = executor.makeNode(secondFunc);

    second->setParent<0>(first);

    executor.execute();
    std::cout << "Done running\n";
    try {
        auto initial_input = first->collect();
        FAIL() << "Expected std::logic_error";
    }
    catch (const std::logic_error& err) {
        EXPECT_EQ(err.what(), std::string("No result found in node"));
    }
    auto final_output = second->collect();
    EXPECT_EQ(*final_output, 6);
}

TEST_F(GraphExTest, ShouldThrowIfNonCopyableObjectIsPassedToMoreThanOneChild)
{
    GraphEx executor;

    using NonCopyableType = std::unique_ptr<int>;
    std::function<NonCopyableType()> firstFunc = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = executor.makeNode(firstFunc);

    std::function<NonCopyableType(NonCopyableType)> secondFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = executor.makeNode(secondFunc);

    std::function<NonCopyableType(NonCopyableType)> thirdFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 9;
        return a;
    };
    decltype(auto) third = executor.makeNode(thirdFunc);

    second->setParent<0>(first);
    try {
        third->setParent<0>(first);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::logic_error& err) {
        EXPECT_EQ(err.what(),
                  std::string("Non copyable result cannot be passed to more "
                              "than 1 child process"));
    }
}

TEST_F(GraphExTest, ShouldBeAbleToAddStructMethod)
{
    GraphEx executor;

    struct Foo {
        auto first() -> int { return 4; }
        auto second(int x) -> int { return x * 2; }
    };
    Foo foo;
    std::function<int(void)> firstFunc = std::bind(&Foo::first, &foo);
    decltype(auto) first = executor.makeNode(firstFunc);

    std::function<int(int)> secondFunc =
        std::bind(&Foo::second, &foo, std::placeholders::_1);
    decltype(auto) second = executor.makeNode(secondFunc);

    second->setParent<0>(first);

    executor.execute();

    EXPECT_EQ(second->collect(), 8);
}

TEST_F(GraphExTest, ShouldBeAbleToRunConcurrentlyCorrectly)
{
    int oneThreadResult = -1, twoThreadResult = -2, fourThreadResult = -4,
        eightThreadResult = -8;
    std::function<void(void)> firstFunc = []() -> void {};
    std::function<int(void)> secondFunc = []() -> int { return 1; };
    std::function<int(int)> thirdFunc = [](int a) -> int { return a + 2; };
    std::function<int(int)> fourthFunc = [](int a) -> int { return a * 2; };
    std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
        return a % b;
    };

    {  // 1 thread
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

        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third->collect(), 3);
        EXPECT_EQ(fourth->collect(), 2);
        oneThreadResult = fifth->collect();
    }

    {  // 2 thread
        GraphEx executor(2);

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

        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third->collect(), 3);
        EXPECT_EQ(fourth->collect(), 2);
        twoThreadResult = fifth->collect();
    }

    {  // 4 thread
        GraphEx executor(4);

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

        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third->collect(), 3);
        EXPECT_EQ(fourth->collect(), 2);
        fourThreadResult = fifth->collect();
    }

    {  // 8 thread
        GraphEx executor(8);

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

        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third->collect(), 3);
        EXPECT_EQ(fourth->collect(), 2);
        eightThreadResult = fifth->collect();
    }
    EXPECT_EQ(oneThreadResult, twoThreadResult);
    EXPECT_EQ(fourThreadResult, twoThreadResult);
    EXPECT_EQ(fourThreadResult, eightThreadResult);
}

TEST_F(GraphExTest, ShouldBeAbleToRunConcurrentlyCorrectly2)
{
    constexpr int loop_n = 10'000;
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
            if (b & 1)
                ret = (long long)ret * a % MOD;
            a = (long long)a * a % MOD;
            b >>= 1;
        }
        return ret;
    };

    std::function<int(int, int, int, int)> sixCostlyFunc =
        [](int a, int b, int c, int d) -> int {
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
    };

    GraphEx executor(4);

    decltype(auto) first = executor.makeNode(secondCostlyFunc);
    decltype(auto) second = executor.makeNode(thirdCostlyFunc);
    decltype(auto) third = executor.makeNode(thirdCostlyFunc);
    decltype(auto) fourth = executor.makeNode(fourthCostlyFunc);
    decltype(auto) fifth = executor.makeNode(fourthCostlyFunc);
    decltype(auto) sixth = executor.makeNode(sixCostlyFunc);

    second->setParent<0>(first);
    third->setParent<0>(first);
    fourth->setParent<0>(first);
    fifth->setParent<0>(first);

    sixth->setParent<0>(second);
    sixth->setParent<1>(third);
    sixth->setParent<2>(fourth);
    sixth->setParent<3>(fifth);

    executor.execute();
    EXPECT_EQ(sixth->collect(), 123235512);
}

TEST_F(GraphExTest, ResetAndExecuteRepeatedly)
{
    GraphEx executor;

    decltype(auto) first =
        executor.makeNode([]() -> void { std::cout << "Running first\n"; });
    // Need to explicitly set this one or compiler wont be able to deduce
    // whether this is makeNode<void, Args..> or makeNode<ReturnType, ...>
    std::function<int(void)> secondFunc = []() -> int { return 1; };
    decltype(auto) second = executor.makeNode(secondFunc);
    std::function<int(int)> thirdFunc = [](int a) -> int { return a + 2; };
    decltype(auto) third = executor.makeNode(thirdFunc);
    std::function<int(int)> fourthFunc = [](int a) -> int { return a * 2; };
    decltype(auto) fourth = executor.makeNode(fourthFunc);
    std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
        return a % b;
    };
    decltype(auto) fifth = executor.makeNode(fifthFunc);

    second->setParent(first);
    third->setParent<0>(second);
    fourth->setParent<0>(second);
    fifth->setParent<0>(third);
    fifth->setParent<1>(fourth);

    for (uint8_t i = 0; i < 2; ++i) {
        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third->collect(), 3);
        EXPECT_EQ(fourth->collect(), 2);
        EXPECT_EQ(fifth->collect(), 1);
        executor.reset();
    }
}

TEST_F(GraphExTest, ShouldBeAbleToInjectParameterManually)
{
    GraphEx executor;
    std::function<int(int)> secondFunc = [](int a) -> int { return a; };
    decltype(auto) second = executor.makeNode(secondFunc);
    std::function<int(int)> thirdFunc = [](int a) -> int { return a + 2; };
    decltype(auto) third = executor.makeNode(thirdFunc);
    std::function<int(int)> fourthFunc = [](int a) -> int { return a * 2; };
    decltype(auto) fourth = executor.makeNode(fourthFunc);
    std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
        return a % b;
    };
    decltype(auto) fifth = executor.makeNode(fifthFunc);

    third->setParent<0>(second);
    fourth->setParent<0>(second);
    fifth->setParent<0>(third);
    fifth->setParent<1>(fourth);

    second->feed<0>(10);
    executor.execute();
    EXPECT_EQ(fifth->collect(), 12);

    executor.reset();
    second->feed<0>(20);
    executor.execute();
    EXPECT_EQ(fifth->collect(), 22);
}

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}