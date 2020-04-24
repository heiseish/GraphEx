/// Draft 2. Limitations
/// - No concurrency

#include <atomic>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
// #include <concepts>

namespace GE {

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

class BaseNode {
public:
    virtual auto Execute() -> void = 0;

    /// @brief mark the node as output node and preserve the value returned by
    /// the job function. If a node is not marked as output node, there is no
    /// guarantee The result retrieved from the node is truthful
    virtual auto MarkAsOutput() -> void { _isOutput = true; }
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

    template <std::size_t idx, typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        parent.AddChild(std::bind(&Node::OnArgumentReady<idx>, this,
                                  std::placeholders::_1));
        parent.NextNodes.emplace_back(this);
    }

    auto AddChild(SubscribeCallback child) -> void
    {
        childTasks.push_back(child);
    }

    template <std::size_t idx>
    auto OnArgumentReady(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        std::get<idx>(args) = arg;
        if (!--pendingCount) Execute();
    }

    // Direct dependency but not argument required
    template <typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        ++pendingCount;
        parent.AddChild(std::bind(
            static_cast<void (Node::*)(void)>(&Node::OnArgumentReady), this));
        parent.NextNodes.emplace_back(this);
    }

    auto AddChild(SubscribeNoArgCallback child) -> void
    {
        childTasks.push_back(child);
    }

    auto OnArgumentReady() -> void
    {
        if (!--pendingCount) Execute();
    }

    auto Execute() -> void override
    {
        _result = std::apply(job, args);
        for (size_t i = 0; i < childTasks.size(); ++i) {
            if (i == childTasks.size() - 1 && !_isOutput &&
                std::is_move_constructible<ReturnType>::value)
                // move the result since the result will be no longer needed
                childTasks[i](std::move(_result.value()));
            else
                childTasks[i](_result.value());
        }
    }

    auto Collect() -> ReturnType
    {
        if (!_result.has_value()) {
            throw std::runtime_error("No result found in node");
        }
        return _result.value();
    }

private:
    JobCallback job;
    std::tuple<Args...> args;
    std::optional<ReturnType> _result;
    size_t pendingCount = std::tuple_size<std::tuple<Args...>>::value;
    std::vector<SubscribeCallback> childTasks;
};

template <typename... Args>
class Node<void, Args...> final : public BaseNode {
    friend class GraphEx;

public:
    using JobCallback = std::function<void(Args...)>;
    using SubscribeCallback = std::function<void(void)>;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(JobCallback j) : job(j) {}

    // for GraphEx
    template <std::size_t idx, typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        parent.AddChild(std::bind(&Node::OnArgumentReady<idx>, this,
                                  std::placeholders::_1));
        parent.NextNodes.emplace_back(this);
    }

    // Direct dependency but not argument required
    template <typename ParentTask>
    auto SetParent(ParentTask& parent) -> void
    {
        ++pendingCount;
        parent.AddChild(std::bind(
            static_cast<void (Node::*)(void)>(&Node::OnArgumentReady), this));
        parent.NextNodes.emplace_back(this);
    }

    auto AddChild(SubscribeCallback child) -> void
    {
        childTasks.push_back(child);
    }

    template <std::size_t idx>
    auto OnArgumentReady(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        std::get<idx>(args) = arg;
        if (!--pendingCount) Execute();
    }

    auto OnArgumentReady() -> void
    {
        if (!--pendingCount) Execute();
    }

    auto Execute() -> void override
    {
        std::apply(job, args);
        for (auto childTask : childTasks) childTask();
    }

    auto Collect() -> void {}

private:
    JobCallback job;
    std::tuple<Args...> args;
    size_t pendingCount = std::tuple_size<std::tuple<Args...>>::value;
    std::vector<SubscribeCallback> childTasks;
};

class GraphExOptions {
    int MaxConcurrency = 4;
};

class GraphEx {
public:
    GraphEx(GraphExOptions options) noexcept : _options(options) {}

    /// @brief register entry point for the graph
    template <typename ReturnType, typename... Args>
    auto RegisterInputNodes(std::shared_ptr<Node<ReturnType, Args...>> node)
        -> void
    {
        _input_nodes.emplace_back(node);
    }

    template <typename... Args>
    auto RegisterInputNodes(std::shared_ptr<Node<void, Args...>> node) -> void
    {
        _input_nodes.emplace_back(node);
    }

    // template <typename ReturnType>
    // auto RegisterInputNodes(std::shared_ptr<Node<ReturnType>> node) -> void {
    //     _input_nodes.emplace_back(node);
    // }

    auto RegisterInputNodes(std::shared_ptr<Node<void>> node) -> void
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
            if (dfs(input_node.get())) {
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
    std::vector<std::shared_ptr<BaseNode>> _input_nodes;
};

template <typename ReturnType, typename... Args>
decltype(auto) MakeNode(std::function<ReturnType(Args...)> func)
{
    return std::make_shared<Node<ReturnType, Args...>>(func);
}

template <typename... Args>
decltype(auto) MakeNode(std::function<void(Args...)> func)
{
    return std::make_shared<Node<void, Args...>>(func);
}

template <typename ReturnType>
decltype(auto) MakeNode(std::function<ReturnType(void)> func)
{
    return std::make_shared<Node<ReturnType>>(func);
}

decltype(auto) MakeNode(std::function<void(void)> func)
{
    return std::make_shared<Node<void>>(func);
}

}  // namespace GE
