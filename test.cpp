#include "graphex.hpp"
#include "gtest/gtest.h"

using namespace GE;

class GraphExTest : public ::testing::Test {
};

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph)
{
    GraphEx executor;
    decltype(auto) first =
        makeNode(executor, []() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        makeNode(executor, []() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        makeNode(executor, []() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        makeNode(executor, []() -> void { std::cout << "Running fourth\n"; });

    second.setParent(first);
    fourth.setParent(first);
    second.setParent(third);
    fourth.setParent(third);
    third.setParent(first);
    
    executor.registerInputNode(&first);
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
    decltype(auto) first = makeNode(executor, firstFunc);
    decltype(auto) second =
        makeNode(executor, []() -> void { std::cout << "Running second\n"; });
    second.setParent(first);

    executor.registerInputNode(&first);
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
        makeNode(executor, []() -> void { std::cout << "Running first\n"; });

    // Need to explicitly set this one or compiler wont be able to deduce
    // whether this is makeNode<void, Args..> or makeNode<ReturnType, ...>
    std::function<int(void)> secondFunc = []() -> int {
        std::cout << "Running second\nReturn 1\n";
        return 1;
    };
    decltype(auto) second = makeNode(executor, secondFunc);

    // Need to explicitly create std::function, seems like
    // template is unable to deduce the type of lambda
    std::function<int(int)> thirdFunc = [](int a) -> int {
        std::cout << "Running third\nAdding 2: a + 2 == " << a + 2 << "\n";
        return a + 2;
    };
    decltype(auto) third = makeNode(executor, thirdFunc);

    // same as above
    std::function<int(int)> fourthFunc = [](int a) -> int {
        std::cout << "Running fourth\nMultiplying by 2: a * 2 == " << a * 2
                  << "\n";
        return a * 2;
    };
    decltype(auto) fourth = makeNode(executor, fourthFunc);

    std::function<int(int, int)> fifthFunc = [](int a, int b) -> int {
        std::cout << "Running fifth\nModding the two numbers: a % b == "
                  << a % b << "\n";
        return a % b;
    };
    decltype(auto) fifth = makeNode(executor, fifthFunc);

    second.setParent(first);
    third.setParent<0>(second);
    fourth.setParent<0>(second);
    fifth.setParent<0>(third);
    fifth.setParent<1>(fourth);

    executor.registerInputNode(&first);
    EXPECT_FALSE(executor.hasCycle());
    executor.execute();
    EXPECT_EQ(third.collect(), 3);
    EXPECT_EQ(fourth.collect(), 2);
    EXPECT_EQ(fifth.collect(), 1);
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
        makeNode(executor, []() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        makeNode(executor, []() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        makeNode(executor, []() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        makeNode(executor, []() -> void { std::cout << "Running fourth\n"; });
    second.setParent(first);
    third.setParent(second);
    fourth.setParent(third);
    first.setParent(fourth);

    executor.registerInputNode(&first);
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

        decltype(auto) preprocess =
            makeNode(executor, []() -> void { std::cout << "Running preprocessing\n"; });

        std::function<MyMoveable()> firstFunc = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = makeNode(executor, firstFunc);
        std::function<MyMoveable(MyMoveable)> secondFunc =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "oh shit";
            return a;
        };
        decltype(auto) second = makeNode(executor, secondFunc);
        second.setParent(preprocess);
        second.setParent<0>(first);
        second.markAsOutput();

        executor.registerInputNode(&first);
        executor.registerInputNode(&preprocess);
        executor.execute();
        auto initial_input = first.collect();
    }

    {
        GraphEx executor;

        decltype(auto) preprocess =
            makeNode(executor, []() -> void { std::cout << "Running preprocessing\n"; });
        std::function<MyMoveable()> firstFunc = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = makeNode(executor, firstFunc);
        std::function<MyMoveable(MyMoveable)> secondFunc =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "just updated";
            return a;
        };
        decltype(auto) second = makeNode(executor, secondFunc);
        second.setParent(preprocess);
        second.setParent<0>(first);
        second.markAsOutput();
        first.markAsOutput();

        executor.registerInputNode(&first);
        executor.registerInputNode(&preprocess);
        executor.execute();
        auto final_output = second.collect();
        EXPECT_EQ(final_output.rand_str, "just updated");
        auto initial_input = first.collect();
        EXPECT_EQ(initial_input.rand_str, "hello universe");
    }
}

TEST_F(GraphExTest, ShouldBeAbleToHandleNonCopyableStruct)
{
    using NonCopyableType = std::unique_ptr<int>;
    GraphEx executor;

    std::function<NonCopyableType()> firstFunc = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = makeNode(executor, firstFunc);
    std::function<NonCopyableType(NonCopyableType)> secondFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = makeNode(executor, secondFunc);
    second.setParent<0>(first);
    second.markAsOutput();

    executor.registerInputNode(&first);
    executor.execute();
    std::cout << "Done running\n";
    try {
        auto initial_input = first.collect();
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& err) {
        EXPECT_EQ(err.what(), std::string("No result found in node"));
    }
    auto final_output = second.collect();
    EXPECT_EQ(*final_output, 6);
}

TEST_F(GraphExTest, ShouldThrowIfNonCopyableObjectIsPassedToMoreThanOneChild)
{
    using NonCopyableType = std::unique_ptr<int>;
    GraphEx executor;

    std::function<NonCopyableType()> firstFunc = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = makeNode(executor, firstFunc);

    std::function<NonCopyableType(NonCopyableType)> secondFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = makeNode(executor, secondFunc);

    std::function<NonCopyableType(NonCopyableType)> thirdFunc =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 9;
        return a;
    };
    decltype(auto) third = makeNode(executor, thirdFunc);
    second.setParent<0>(first);
    try {
        third.setParent<0>(first);
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
    struct Foo {
        auto first() -> int { return 4; }
        auto second(int x) -> int { return x * 2; }
    };

    Foo foo;
    GraphEx executor;

    std::function<int(void)> firstFunc = std::bind(&Foo::first, &foo);
    decltype(auto) first = makeNode(executor, firstFunc);

    std::function<int(int)> secondFunc =
        std::bind(&Foo::second, &foo, std::placeholders::_1);
    decltype(auto) second = makeNode(executor, secondFunc);

    second.setParent<0>(first);
    second.markAsOutput();

    executor.registerInputNode(&first);
    executor.execute();

    EXPECT_EQ(second.collect(), 8);
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
        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        oneThreadResult = fifth.collect();
    }

    {  // 2 thread
        GraphEx executor(2);

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
        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        twoThreadResult = fifth.collect();
    }

    {  // 4 thread
        GraphEx executor(4);

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
        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        fourThreadResult = fifth.collect();
    }

    {  // 8 thread
        GraphEx executor(8);
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
        EXPECT_FALSE(executor.hasCycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        eightThreadResult = fifth.collect();
    }
    EXPECT_EQ(oneThreadResult, twoThreadResult);
    EXPECT_EQ(fourThreadResult, twoThreadResult);
    EXPECT_EQ(fourThreadResult, eightThreadResult);
}

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}