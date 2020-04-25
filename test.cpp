#include "graphex.hpp"
#include "gtest/gtest.h"

using namespace GE;

class GraphExTest : public ::testing::Test {
};

TEST_F(GraphExTest, ShouldBeAbleToRunSimpleChainGraph)
{
    decltype(auto) first =
        make_node([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        make_node([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        make_node([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        make_node([]() -> void { std::cout << "Running fourth\n"; });
    second.set_parent(first);
    fourth.set_parent(first);
    second.set_parent(third);
    fourth.set_parent(third);
    third.set_parent(first);
    GraphEx executor;
    executor.register_input_node(&first);
    EXPECT_FALSE(executor.has_cycle());
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
    std::function<int(void)> first_func = []() -> int { return 1; };
    decltype(auto) first = make_node(first_func);
    decltype(auto) second =
        make_node([]() -> void { std::cout << "Running second\n"; });
    second.set_parent(first);
    GraphEx executor;
    executor.register_input_node(&first);
    EXPECT_FALSE(executor.has_cycle());
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
    decltype(auto) first =
        make_node([]() -> void { std::cout << "Running first\n"; });

    // Need to explicitly set this one or compiler wont be able to deduce
    // whether this is make_node<void, Args..> or make_node<ReturnType, ...>
    std::function<int(void)> second_func = []() -> int {
        std::cout << "Running second\nReturn 1\n";
        return 1;
    };
    decltype(auto) second = make_node(second_func);

    // Need to explicitly create std::function, seems like
    // template is unable to deduce the type of lambda
    std::function<int(int)> third_func = [](int a) -> int {
        std::cout << "Running third\nAdding 2: a + 2 == " << a + 2 << "\n";
        return a + 2;
    };
    decltype(auto) third = make_node(third_func);

    // same as above
    std::function<int(int)> fourth_func = [](int a) -> int {
        std::cout << "Running fourth\nMultiplying by 2: a * 2 == " << a * 2
                  << "\n";
        return a * 2;
    };
    decltype(auto) fourth = make_node(fourth_func);

    std::function<int(int, int)> fifth_func = [](int a, int b) -> int {
        std::cout << "Running fifth\nModding the two numbers: a % b == "
                  << a % b << "\n";
        return a % b;
    };
    decltype(auto) fifth = make_node(fifth_func);

    second.set_parent(first);
    third.set_parent<0>(second);
    fourth.set_parent<0>(second);
    fifth.set_parent<0>(third);
    fifth.set_parent<1>(fourth);
    GraphEx executor;
    executor.register_input_node(&first);
    EXPECT_FALSE(executor.has_cycle());
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
    decltype(auto) first =
        make_node([]() -> void { std::cout << "Running first\n"; });
    decltype(auto) second =
        make_node([]() -> void { std::cout << "Running second\n"; });
    decltype(auto) third =
        make_node([]() -> void { std::cout << "Running third\n"; });
    decltype(auto) fourth =
        make_node([]() -> void { std::cout << "Running fourth\n"; });
    second.set_parent(first);
    third.set_parent(second);
    fourth.set_parent(third);
    first.set_parent(fourth);
    GraphEx executor;
    executor.register_input_node(&first);
    EXPECT_TRUE(executor.has_cycle());
}

TEST_F(GraphExTest, ShouldBeAbleToHandleMovableObjectCorrectly)
{
    struct MyMoveable {
        int i = 1;
        std::string rand_str = "hello universe";
    };

    {
        decltype(auto) preprocess =
            make_node([]() -> void { std::cout << "Running preprocessing\n"; });

        std::function<MyMoveable()> first_func = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = make_node(first_func);
        std::function<MyMoveable(MyMoveable)> second_func =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "oh shit";
            return a;
        };
        decltype(auto) second = make_node(second_func);
        second.set_parent(preprocess);
        second.set_parent<0>(first);
        second.mark_as_output();
        GraphEx executor;
        executor.register_input_node(&first);
        executor.register_input_node(&preprocess);
        executor.execute();
        auto initial_input = first.collect();
    }

    {
        decltype(auto) preprocess =
            make_node([]() -> void { std::cout << "Running preprocessing\n"; });
        std::function<MyMoveable()> first_func = []() -> MyMoveable {
            return {};
        };
        decltype(auto) first = make_node(first_func);
        std::function<MyMoveable(MyMoveable)> second_func =
            [](MyMoveable a) -> MyMoveable {
            a.rand_str = "just updated";
            return a;
        };
        decltype(auto) second = make_node(second_func);
        second.set_parent(preprocess);
        second.set_parent<0>(first);
        second.mark_as_output();
        first.mark_as_output();
        GraphEx executor;
        executor.register_input_node(&first);
        executor.register_input_node(&preprocess);
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
    std::function<NonCopyableType()> first_func = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = make_node(first_func);
    std::function<NonCopyableType(NonCopyableType)> second_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = make_node(second_func);
    second.set_parent<0>(first);
    second.mark_as_output();
    GraphEx executor;
    executor.register_input_node(&first);
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
    std::function<NonCopyableType()> first_func = []() -> NonCopyableType {
        return std::make_unique<int>(10);
    };
    decltype(auto) first = make_node(first_func);

    std::function<NonCopyableType(NonCopyableType)> second_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 6;
        return a;
    };
    decltype(auto) second = make_node(second_func);

    std::function<NonCopyableType(NonCopyableType)> third_func =
        [](NonCopyableType a) -> NonCopyableType {
        *a = 9;
        return a;
    };
    decltype(auto) third = make_node(third_func);
    second.set_parent<0>(first);
    try {
        third.set_parent<0>(first);
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
    std::function<int(void)> first_func = std::bind(&Foo::first, &foo);
    decltype(auto) first = make_node(first_func);

    std::function<int(int)> second_func =
        std::bind(&Foo::second, &foo, std::placeholders::_1);
    decltype(auto) second = make_node(second_func);

    second.set_parent<0>(first);
    second.mark_as_output();
    GraphEx executor;
    executor.register_input_node(&first);
    executor.execute();

    EXPECT_EQ(second.collect(), 8);
}

TEST_F(GraphExTest, ShouldBeAbleToRunConcurrentlyCorrectly)
{
    int one_thread_result = -1, two_thread_result = -2, four_thread_result = -4,
        eight_thread_result = -8;
    std::function<void(void)> first_func = []() -> void {};
    std::function<int(void)> second_func = []() -> int { return 1; };
    std::function<int(int)> third_func = [](int a) -> int { return a + 2; };
    std::function<int(int)> fourth_func = [](int a) -> int { return a * 2; };
    std::function<int(int, int)> fifth_func = [](int a, int b) -> int {
        return a % b;
    };

    {  // 1 thread
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
        EXPECT_FALSE(executor.has_cycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        one_thread_result = fifth.collect();
    }

    {  // 2 thread
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
        GraphEx executor(2);
        executor.register_input_node(&first);
        EXPECT_FALSE(executor.has_cycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        two_thread_result = fifth.collect();
    }

    {  // 4 thread
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
        GraphEx executor(4);
        executor.register_input_node(&first);
        EXPECT_FALSE(executor.has_cycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        four_thread_result = fifth.collect();
    }

    {  // 8 thread
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
        GraphEx executor(8);
        executor.register_input_node(&first);
        EXPECT_FALSE(executor.has_cycle());
        executor.execute();
        EXPECT_EQ(third.collect(), 3);
        EXPECT_EQ(fourth.collect(), 2);
        eight_thread_result = fifth.collect();
    }
    EXPECT_EQ(one_thread_result, two_thread_result);
    EXPECT_EQ(four_thread_result, two_thread_result);
    EXPECT_EQ(four_thread_result, eight_thread_result);
}

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}