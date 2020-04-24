#include "graphex.hpp"
#include "gtest/gtest.h"

using namespace GE;

class GraphExTest : public ::testing::Test {
};

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph)
{
    decltype(auto) first =
        MakeNode([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        MakeNode([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        MakeNode([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        MakeNode([]() -> void { std::cout << "Running fourth\n"; });
    second.SetParent(first);
    fourth.SetParent(first);
    second.SetParent(third);
    fourth.SetParent(third);
    third.SetParent(first);
    GraphExOptions opt;
    GraphEx executor(opt);
    executor.RegisterInputNodes(&first);
    EXPECT_FALSE(executor.HasCycle());
    executor.Execute();

    /**
    Running first
    Running third
    Running second
    Running fourth
    */
}

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph2)
{
    std::function<int(void)> first_func = []() -> int { return 1; };
    decltype(auto) first = MakeNode(first_func);
    decltype(auto) second =
        MakeNode([]() -> void { std::cout << "Running second\n"; });
    second.SetParent(first);
    GraphExOptions opt;
    GraphEx executor(opt);
    executor.RegisterInputNodes(&first);
    EXPECT_FALSE(executor.HasCycle());
    executor.Execute();
    /**
    Running first
    Running third
    Running second
    Running fourth
    */
}

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleGraphWithArgumentPassing)
{
    decltype(auto) first =
        MakeNode([]() -> void { std::cout << "Running first\n"; });

    // Need to explicitly set this one or compiler wont be able to deduce
    // whether this is MakeNode<void, Args..> or MakeNode<ReturnType, ...>
    std::function<int(void)> second_func = []() -> int {
        std::cout << "Running second\nReturn 1\n";
        return 1;
    };
    decltype(auto) second = MakeNode(second_func);

    // Need to explicitly create std::function, seems like
    // template is unable to deduce the type of lambda
    std::function<int(int)> third_func = [](int a) -> int {
        std::cout << "Running third\nAdding 2: a + 2 == " << a + 2 << "\n";
        return a + 2;
    };
    decltype(auto) third = MakeNode(third_func);

    // same as above
    std::function<int(int)> fourth_func = [](int a) -> int {
        std::cout << "Running fourth\nMultiplying by 2: a * 2 == " << a * 2
                  << "\n";
        return a * 2;
    };
    decltype(auto) fourth = MakeNode(fourth_func);

    std::function<int(int, int)> fifth_func = [](int a, int b) -> int {
        std::cout << "Running fifth\nModding the two numbers: a % b == "
                  << a % b << "\n";
        return a % b;
    };
    decltype(auto) fifth = MakeNode(fifth_func);

    second.SetParent(first);
    third.SetParent<0>(second);
    fourth.SetParent<0>(second);
    fifth.SetParent<0>(third);
    fifth.SetParent<1>(fourth);
    GraphExOptions opt;
    GraphEx executor(opt);
    executor.RegisterInputNodes(&first);
    EXPECT_FALSE(executor.HasCycle());
    executor.Execute();
    EXPECT_EQ(third.Collect(), 3);
    EXPECT_EQ(fourth.Collect(), 2);
    EXPECT_EQ(fifth.Collect(), 1);
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
    decltype(auto) first =
        MakeNode([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        MakeNode([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        MakeNode([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        MakeNode([]() -> void { std::cout << "Running fourth\n"; });
    second.SetParent(first);
    third.SetParent(second);
    fourth.SetParent(third);
    first.SetParent(fourth);
    GraphExOptions opt;
    GraphEx executor(opt);
    executor.RegisterInputNodes(&first);
    EXPECT_TRUE(executor.HasCycle());
}

TEST_F(GraphExTest, OptimizePassingArgumentByMoving)
{
    struct MyMoveable {
        int i = 1;
        std::string rand_str = "hello universe";
    };

    {
        decltype(auto) preprocess =
            MakeNode([]() -> void { std::cout << "Running preprocessing\n"; });

        std::function<MyMoveable()> first_func = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = MakeNode(first_func);
        std::function<MyMoveable(MyMoveable)> second_func =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "oh shit";
            return a;
        };
        decltype(auto) second = MakeNode(second_func);
        second.SetParent(preprocess);
        second.SetParent<0>(first);
        second.MarkAsOutput();
        GraphExOptions opt;
        GraphEx executor(opt);
        executor.RegisterInputNodes(&first);
        executor.RegisterInputNodes(&preprocess);
        executor.Execute();
        try {
            // first MakeNode is not marked output during execution so the
            // result from first func is not kept but moved to second func
            auto initial_input = first.Collect();
            FAIL() << "Expected std::runtime_error";
        }
        catch (const std::runtime_error& err) {
            EXPECT_EQ(err.what(), std::string("No result found in node"));
        }
    }

    {
        decltype(auto) preprocess =
            MakeNode([]() -> void { std::cout << "Running preprocessing\n"; });
        std::function<MyMoveable()> first_func = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = MakeNode(first_func);
        std::function<MyMoveable(MyMoveable)> second_func =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "just updated";
            return a;
        };
        decltype(auto) second = MakeNode(second_func);
        second.SetParent(preprocess);
        second.SetParent<0>(first);
        second.MarkAsOutput();
        first.MarkAsOutput();
        GraphExOptions opt;
        GraphEx executor(opt);
        executor.RegisterInputNodes(&first);
        executor.RegisterInputNodes(&preprocess);
        executor.Execute();
        auto final_output = second.Collect();
        EXPECT_EQ(final_output.rand_str, "just updated");
        auto initial_input = first.Collect();
        EXPECT_EQ(initial_input.rand_str, "hello universe");
    }
}

TEST_F(GraphExTest, ShouldBeAbleToHandleNonCopyableStruct)
{
    using NonCopyableType = std::unique_ptr<int>;
    std::function<NonCopyableType()> first_func = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = MakeNode(first_func);
    std::function<NonCopyableType(NonCopyableType)> second_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = MakeNode(second_func);
    second.SetParent<0>(first);
    second.MarkAsOutput();
    GraphExOptions opt;
    GraphEx executor(opt);
    executor.RegisterInputNodes(&first);
    executor.Execute();
    std::cout << "Done running\n";
    try {
        auto initial_input = first.Collect();
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& err) {
        EXPECT_EQ(err.what(), std::string("No result found in node"));
    }
    auto final_output = second.Collect();
    EXPECT_EQ(*final_output, 6);
}

TEST_F(GraphExTest, ShouldThrowIfNonCopyableObjectIsPassedToMoreThanOneChild)
{
    using NonCopyableType = std::unique_ptr<int>;
    std::function<NonCopyableType()> first_func = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = MakeNode(first_func);

    std::function<NonCopyableType(NonCopyableType)> second_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = MakeNode(second_func);

    std::function<NonCopyableType(NonCopyableType)> third_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 9;
        return a;
    };
    decltype(auto) third = MakeNode(third_func);
    second.SetParent<0>(first);
    try {
        third.SetParent<0>(first);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::logic_error& err) {
        EXPECT_EQ(err.what(),
                  std::string("Non copyable result cannot be passed to more "
                              "than 1 child process"));
    }
}

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}