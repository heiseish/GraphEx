/// Draft 2. Limitations
/// - No concurrency
#pragma once

#ifndef GRAPH_EX_H
#define GRAPH_EX_H

#include <atomic>
#include <functional>
#include <iostream>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GE {

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define DE_ENFORCE(x, y)                       \
    do                                         \
        if (!(x)) throw std::runtime_error(y); \
    while (0)

class BaseNode {
public:
    virtual auto Execute() -> void = 0;

    /// @brief MarkAsOutput mark the node as output node and preserve the value
    /// returned by the job function. If a node is not marked as output node,
    /// there is no guarantee The result retrieved from the node is valid
    virtual auto MarkAsOutput() -> void { _isOutput = true; }

    /// NextNodes contains the child nodes for current node. Those are nodes
    /// which are signal upon the completion of the job in current nnode
    std::vector<BaseNode*> NextNodes;

protected:
    bool _isOutput = false;
};

template <typename ReturnType, typename... Args>
class Node final : public BaseNode {
    friend class GraphEx;

public:
    using JobCallback = std::function<ReturnType(Args...)>;
    using SubscribeCallback = std::function<void(ReturnType)>;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(JobCallback j) : job(j) {}

    /// @brief SetParent add a node as a prequel to current node, and the result
    /// of parent node will be passed or consumed by current node once the
    /// parent node job is done
    /// @param idx template argument of the position of result object in current
    /// node's `args` For example, let's say there are 3 functions, whose
    /// signatures as followed
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// c = [](int x, int y) -> int {}
    /// ```
    /// We need to know whether the result returned by a is passed as `x` or `y`
    /// to c. Using `SetParent`, we can specify by
    /// ```
    /// nodeC->SetParent<0>(*nodeA) // if result by a is passed as x or
    /// nodeC->SetParent<1>(*nodeA) // if result by a is passed as y or
    /// ```
    /// @param parent parent node to be added
    template <std::size_t idx, typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        parent.AddChild(std::bind(&Node::onArgumentReady<idx>, this,
                                  std::placeholders::_1));
        parent.NextNodes.emplace_back(this);
    }

    /// @brief SetParent add a node as a prequel to current node. The parent
    /// won't be passing any result needed by current node, but it still
    /// requires the parent node job to run first before running. For example,
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// nodeA->SetParent(*nodeB);
    /// ```
    /// @param parent parent node to be added
    template <typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        ++pendingCount;
        SubscribeNoArgCallback cb = std::bind(
            static_cast<void (Node::*)(void)>(&Node::onArgumentReady), this);
        parent.AddChild(cb);
        parent.NextNodes.emplace_back(this);
    }

    /// @brief Run the main job registered by current node
    /// After the job is finish, call the registered callback functions
    /// @throw if `ReturnType` is non-copyable but there are more than 1 child
    /// tasks that require the result object
    auto Execute() -> void override
    {
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            DE_ENFORCE(childTasks.size() <= 1,
                       "Internal Error: More than 1 child process for "
                       "non-copyable object");
            _result = std::apply(job, std::move(args));
            _validResult = true;
            if (!childTasks.empty()) {
                childTasks[0](std::move(_result.value()));
                _validResult = false;
            }
        }
        else {
            _result = std::apply(job, args);
            _validResult = true;
            for (size_t i = 0; i < childTasks.size(); ++i) {
                if constexpr (std::is_integral<ReturnType>::value ||
                              !std::is_move_constructible<ReturnType>::value) {
                    childTasks[i](_result.value());
                }
                else {
                    if (i == childTasks.size() - 1 && !_isOutput) {
                        // OPT: move the result since the result will be no
                        // longer needed
                        childTasks[i](std::move(_result.value()));
                        _validResult = false;
                    }
                    else
                        childTasks[i](_result.value());
                }
            }
        }
        for (auto childTask : noArgChildTasks) childTask();
    }

    /// @brief Retrieve the result obtained by the current node job
    /// @throw std::runtime_error if No valid result can be retrieved in the
    /// node
    auto Collect() -> ReturnType
    {
        if (!_validResult) {
            throw std::runtime_error("No result found in node");
        }
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            return std::move(_result.value());
        }
        else {
            return _result.value();
        }
    }

    /// @brief Add a callback function that will be called when the job in
    /// current node finished running. The object generated by the job in thise
    /// node will be passed to or consumed by the callback function `child`
    /// @param child callback function
    auto AddChild(SubscribeCallback child) -> void
    {
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            if (childTasks.size() > 0) {
                throw std::logic_error(
                    "Non copyable result cannot be passed to more than 1 child "
                    "process");
            }
        }
        childTasks.push_back(child);
    }

    /// @brief Add a callback function that will be called when the job in
    /// current node finished running. No argument will be passed to the
    /// callback function
    /// @param child callback function
    auto AddChild(SubscribeNoArgCallback child) -> void
    {
        noArgChildTasks.push_back(child);
    }

private:
    /// @brief A register function that can be used to register callback when
    /// parent nodes have finish running
    template <std::size_t idx>
    auto onArgumentReady(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        if constexpr (!std::is_copy_constructible<decltype(arg)>::value) {
            std::get<idx>(args) = std::move(arg);
        }
        else
            std::get<idx>(args) = arg;
        if (!--pendingCount) Execute();
    }

    auto onArgumentReady() -> void
    {
        if (!--pendingCount) Execute();
    }

    JobCallback job;
    std::tuple<Args...> args;
    bool _validResult = false;
    std::optional<ReturnType> _result;
    size_t pendingCount = std::tuple_size<std::tuple<Args...>>::value;
    std::vector<SubscribeCallback> childTasks;
    std::vector<SubscribeNoArgCallback> noArgChildTasks;
};

template <typename... Args>
class Node<void, Args...> final : public BaseNode {
    friend class GraphEx;

public:
    using JobCallback = std::function<void(Args...)>;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(JobCallback j) : job(j) {}

    /// @brief SetParent add a node as a prequel to current node, and the result
    /// of parent node will be passed or consumed by current node once the
    /// parent node job is done
    /// @param idx template argument of the position of result object in current
    /// node's `args` For example, let's say there are 3 functions, whose
    /// signatures as followed
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// c = [](int x, int y) -> int {}
    /// ```
    /// We need to know whether the result returned by a is passed as `x` or `y`
    /// to c. Using `SetParent`, we can specify by
    /// ```
    /// nodeC->SetParent<0>(*nodeA) // if result by a is passed as x or
    /// nodeC->SetParent<1>(*nodeA) // if result by a is passed as y or
    /// ```
    /// @param parent parent node to be added
    template <std::size_t idx, typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        parent.AddChild(std::bind(&Node::onArgumentReady<idx>, this,
                                  std::placeholders::_1));
        parent.NextNodes.emplace_back(this);
    }

    /// @brief SetParent add a node as a prequel to current node. The parent
    /// won't be passing any result needed by current node, but it still
    /// requires the parent node job to run first before running. For example,
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// nodeA->SetParent(*nodeB);
    /// ```
    /// @param parent parent node to be added
    template <typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        ++pendingCount;
        SubscribeNoArgCallback cb = std::bind(
            static_cast<void (Node::*)(void)>(&Node::onArgumentReady), this);
        parent.AddChild(cb);
        parent.NextNodes.emplace_back(this);
    }

    /// @brief Run the main job registered by current node
    /// After the job is finish, call the registered callback functions
    auto Execute() -> void override
    {
        if constexpr (!std::is_copy_constructible<decltype(args)>::value) {
            std::apply(job, std::move(args));
        }
        else
            std::apply(job, args);
        for (auto childTask : childTasks) childTask();
    }

    auto Collect() -> void {}

    /// @brief Add a callback function that will be called when the job in
    /// current node finished running. The object generated by the job in thise
    /// node will be passed to or consumed by the callback function `child`
    /// @param child callback function
    auto AddChild(SubscribeNoArgCallback child) -> void
    {
        childTasks.push_back(child);
    }

private:
    /// @brief A register function that can be used to register callback when
    /// parent nodes have finish running
    template <std::size_t idx>
    auto onArgumentReady(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        if constexpr (!std::is_copy_constructible<decltype(arg)>::value) {
            std::get<idx>(args) = std::move(arg);
        }
        else
            std::get<idx>(args) = arg;
        if (!--pendingCount) Execute();
    }

    auto onArgumentReady() -> void
    {
        if (!--pendingCount) Execute();
    }

    JobCallback job;
    std::tuple<Args...> args;
    size_t pendingCount = std::tuple_size<std::tuple<Args...>>::value;
    std::vector<SubscribeNoArgCallback> childTasks;
};

class GraphExOptions {
    int MaxConcurrency = 4;
};

class GraphEx {
public:
    GraphEx(GraphExOptions options) noexcept : _options(options) {}

    /// @brief register entry point for the graph
    template <typename ReturnType, typename... Args>
    auto RegisterInputNodes(Node<ReturnType, Args...>* node) -> void
    {
        _input_nodes.emplace_back(node);
    }

    template <typename... Args>
    auto RegisterInputNodes(Node<void, Args...>* node) -> void
    {
        _input_nodes.emplace_back(node);
    }

    auto RegisterInputNodes(Node<void>* node) -> void
    {
        _input_nodes.emplace_back(node);
    }

    /// @brief check for cycle in the graph
    /// assume all the relevant nodes can be checked from input nodes
    auto HasCycle() -> bool
    {
        std::unordered_map<BaseNode*, bool> col;
        std::function<bool(BaseNode*)> dfs =
            [&](BaseNode* current_node) -> bool {
            col[current_node] = true;
            for (auto& next_node : current_node->NextNodes) {
                if (likely(!col.count(next_node))) {
                    if (dfs(next_node)) {
                        return true;
                    }
                }
                else if (unlikely(col[next_node] == true)) {
                    return true;
                }
            }
            col[current_node] = false;
            return false;
        };
        for (auto& input_node : _input_nodes) {
            col.clear();
            if (dfs(input_node)) {
                return true;
            }
        }
        return false;
    }

    /// @brief run the graph execution from input nodes
    auto Execute() -> void
    {
        for (auto& v : _input_nodes) {
            v->Execute();
        }
    }

private:
    GraphExOptions _options;
    std::atomic<int> _concurrency_count = 1;
    std::vector<BaseNode*> _input_nodes;
};

template <typename ReturnType, typename... Args>
decltype(auto) MakeNode(std::function<ReturnType(Args...)> func)
{
    return Node<ReturnType, Args...>(func);
}

template <typename... Args>
decltype(auto) MakeNode(std::function<void(Args...)> func)
{
    return Node<void, Args...>(func);
}

template <typename ReturnType>
decltype(auto) MakeNode(std::function<ReturnType(void)> func)
{
    return Node<ReturnType>(func);
}

decltype(auto) MakeNode(std::function<void(void)> func)
{
    return Node<void>(func);
}

}  // namespace GE

#endif