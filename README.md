# GraphEx
![Language](https://img.shields.io/badge/language-C%2B%2B-informational.svg?logo=C%2B%2B)
![Status](https://img.shields.io/static/v1.svg?label=Status&message=alpha&color=yellow)

<img src="docs/3-03.svg">

Image Credit [Network Vectors by Vecteezy](https://www.vecteezy.com/free-vector/network)

**A single-file header-only C++17 graph-based execution model for a network of interlinked tasks. Support passing of arguments between each node task.**

## Sample usage:

### Simple example with sequential tasks
```C++
using namespace GE;
// define the tasks that need to be ran
// below are simple tasks to be run in sequence
// first ----> second ----> third ----> fourth
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

GraphExOptions opt; // set graph runtime option
GraphEx executor(opt); // create runtime graph

executor.RegisterInputNodes(&first); // register the entry points for the graph. Can be multiple
EXPECT_FALSE(executor.HasCycle()); // Check if the dependency graph has cycle
executor.Execute();

/**
Running first
Running third
Running second
Running fourth
*/
```

### Example with graph with argument passing between nodes
```C++
decltype(auto) first =
    MakeNode([]() -> void { std::cout << "Running first\n"; });

// create a function that doesn't take in anything and return 1
std::function<int(void)> second_func = []() -> int {
    std::cout << "Running second\nReturn 1\n";
    return 1;
};
decltype(auto) second = MakeNode(second_func);

// create a function that takes in a number and return number + 2
std::function<int(int)> third_func = [](int a) -> int {
    std::cout << "Running third\nAdding 2: a + 2 == " << a + 2 << "\n";
    return a + 2;
};
decltype(auto) third = MakeNode(third_func);

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

// The data flow in graph above can be visualize as followed:
//        void           int                  int
// first ------> second ------->   third   -------->    fifth
//                 |     int                  int         |
//                 ------------>   fourth  -------------->
GraphExOptions opt;
GraphEx executor(opt);
executor.RegisterInputNodes(&first);
EXPECT_FALSE(executor.HasCycle());

/// mark the nodes as output to confirm the results later
third.MarkAsOutput();
fourth.MarkAsOutput();
fifth.MarkAsOutput();

executor.Execute();
// Check the result obtained from the nodes
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
```


### Check if a dependency graph has cycle
```C++
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
EXPECT_TRUE(executor.HasCycle()); // 1 -> 2 -> 3 -> 4 -> 1
```

### Usable with a wide range of `ReturnType`
```C++
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
auto final_output = second.Collect();
EXPECT_EQ(*final_output, 6);
```

### Create Node from struct/class method
```C++
struct Foo {
    auto first() -> int { return 4; }
    auto second(int x) -> int { return x * 2; }
};

Foo foo;
std::function<int(void)> first_func = std::bind(&Foo::first, &foo);
decltype(auto) first = MakeNode(first_func);

std::function<int(int)> second_func =
    std::bind(&Foo::second, &foo, std::placeholders::_1);
decltype(auto) second = MakeNode(second_func);

second.SetParent<0>(first);
second.MarkAsOutput();
GraphExOptions opt;
GraphEx executor(opt);
executor.RegisterInputNodes(&first);
executor.Execute();

EXPECT_EQ(second.Collect(), 8);
```

## Installation
Simply include `graphex.hpp` in your project and make sure build the project with C++17-compatible compiler.


## Development
The project is still under development and still too early for any usage. 

To build the tests `./build.sh -b`

To run the tests `./build.sh -rt`

### TODO
- [ ] Add concurrency to tasks execution

## Contribute
### Current contributors
[Truong Giang](https://github.com/heiseish) and
[Minh Phuc](https://github.com/le-minhphuc).


We welcome contributions! Any PR is welcome.

## Feedback
For any feedback or to report a bug, please file a [GitHub Issue](https://github.com/heiseish/graphex/issues).

***
# License
[MIT License](LICENSE)