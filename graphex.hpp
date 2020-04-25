#pragma once

#ifndef GRAPH_EX_H
#define GRAPH_EX_H

#include <atomic>
#include <functional>
#include <iostream>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <queue>
#include <iostream>

#ifdef USE_BOOST_LOCKLESS_Q
#include "cptl.hpp"
#else
#include "cptl_stl.hpp"
#endif

namespace GE {

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define DE_ENFORCE(x, y)                       \
    do                                         \
        if (!(x)) throw std::runtime_error(y); \
    while (0)

class BaseNode {
public:
    virtual void execute() = 0;

    /// @brief mark_as_output mark the node as output node and preserve the
    /// value returned by the job function. If a node is not marked as output
    /// node, there is no guarantee The result retrieved from the node is valid
    virtual void mark_as_output() { _is_output = true; }

    virtual bool all_parent_enqueued() { return false; }
    virtual void parent_enqueued() = 0;
    virtual void reset() = 0;

    /// next_nodes contains the child nodes for current node. Those are nodes
    /// which are signal upon the completion of the job in current nnode
    std::vector<BaseNode*> next_nodes;

protected:
    bool _is_output = false;
};

template <typename ReturnType, typename... Args>
class Node final : public BaseNode {
    friend class GraphEx;

public:
    using JobCallback = std::function<ReturnType(Args...)>;
    using SubscribeCallback = std::function<void(ReturnType)>;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(JobCallback j) : job(j) {}

    /// @brief set_parent add a node as a prequel to current node, and the
    /// result of parent node will be passed or consumed by current node once
    /// the parent node job is done
    /// @param idx template argument of the position of result object in current
    /// node's `args` For example, let's say there are 3 functions, whose
    /// signatures as followed
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// c = [](int x, int y) -> int {}
    /// ```
    /// We need to know whether the result returned by a is passed as `x` or `y`
    /// to c. Using `set_parent`, we can specify by
    /// ```
    /// nodeC->set_parent<0>(*nodeA) // if result by a is passed as x or
    /// nodeC->set_parent<1>(*nodeA) // if result by a is passed as y or
    /// ```
    /// @param parent parent node to be added
    template <std::size_t idx, typename ParentTask>
    void set_parent(ParentTask& parent)
    {
        parent.add_child(std::bind(&Node::on_argument_ready<idx>, this,
                                   std::placeholders::_1));
        parent.next_nodes.emplace_back(this);
    }

    /// @brief set_parent add a node as a prequel to current node. The parent
    /// won't be passing any result needed by current node, but it still
    /// requires the parent node job to run first before running. For example,
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// nodeA->set_parent(*nodeB);
    /// ```
    /// @param parent parent node to be added
    template <typename ParentTask>
    void set_parent(ParentTask& parent)
    {
        add_parent_count();
        SubscribeNoArgCallback cb = std::bind(
            static_cast<void (Node::*)(void)>(&Node::on_argument_ready), this);
        parent.add_child(cb);
        parent.next_nodes.emplace_back(this);
    }

    /// @brief Run the main job registered by current node
    /// After the job is finish, call the registered callback functions
    /// @throw if `ReturnType` is non-copyable but there are more than 1 child
    /// tasks that require the result object
    void execute() override
    {
        // wait for result to be ready
        while (pendingCount > 0)
            ;
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            DE_ENFORCE(child_tasks.size() <= 1,
                       "Internal Error: More than 1 child process for "
                       "non-copyable object");
            _result = std::apply(job, std::move(args));
            _validResult = true;
            if (!child_tasks.empty()) {
                child_tasks[0](std::move(_result.value()));
                _validResult = false;
            }
        }
        else {
            _result = std::apply(job, args);
            _validResult = true;
            for (size_t i = 0; i < child_tasks.size(); ++i) {
                child_tasks[i](_result.value());
            }
        }
        for (auto childTask : no_arg_child_tasks) childTask();
    }

    /// @brief Retrieve the result obtained by the current node job
    /// @throw std::runtime_error if No valid result can be retrieved in the
    /// node
    ReturnType collect()
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
    void add_child(SubscribeCallback child)
    {
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            if (child_tasks.size() > 0) {
                throw std::logic_error(
                    "Non copyable result cannot be passed to more than 1 child "
                    "process");
            }
            if (_is_output) {
                throw std::logic_error(
                    "Non copyable result which has been marked as output "
                    "cannot have children");
            }
        }
        child_tasks.push_back(child);
    }

    /// @brief Add a callback function that will be called when the job in
    /// current node finished running. No argument will be passed to the
    /// callback function
    /// @param child callback function
    void add_child(SubscribeNoArgCallback child)
    {
        no_arg_child_tasks.push_back(child);
    }

private:
    virtual void parent_enqueued() override { ++parentEnqueued; }
    virtual bool all_parent_enqueued() override
    {
        return parentEnqueued == parentCount;
    }
    void add_parent_count()
    {
        ++parentCount;
        ++pendingCount;
    }
    virtual void reset() override
    {
        _result.reset();
        pendingCount = parentCount;
        parentEnqueued = 0;
    }
    /// @brief A register function that can be used to register callback when
    /// parent nodes have finish running
    template <std::size_t idx>
    auto on_argument_ready(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        if constexpr (!std::is_copy_constructible<decltype(arg)>::value) {
            std::get<idx>(args) = std::move(arg);
        }
        else
            std::get<idx>(args) = arg;
        --pendingCount;
    }

    void on_argument_ready() { --pendingCount; }

    JobCallback job;
    std::tuple<Args...> args;
    bool _validResult = false;
    std::optional<ReturnType> _result;

    size_t parentEnqueued = 0;
    std::atomic<size_t> pendingCount =
        std::tuple_size<std::tuple<Args...>>::value;
    size_t parentCount = std::tuple_size<std::tuple<Args...>>::value;

    std::vector<SubscribeCallback> child_tasks;
    std::vector<SubscribeNoArgCallback> no_arg_child_tasks;
};

template <typename... Args>
class Node<void, Args...> final : public BaseNode {
    friend class GraphEx;

public:
    using JobCallback = std::function<void(Args...)>;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(JobCallback j) : job(j) {}

    /// @brief set_parent add a node as a prequel to current node, and the
    /// result of parent node will be passed or consumed by current node once
    /// the parent node job is done
    /// @param idx template argument of the position of result object in current
    /// node's `args` For example, let's say there are 3 functions, whose
    /// signatures as followed
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// c = [](int x, int y) -> int {}
    /// ```
    /// We need to know whether the result returned by a is passed as `x` or `y`
    /// to c. Using `set_parent`, we can specify by
    /// ```
    /// nodeC->set_parent<0>(*nodeA) // if result by a is passed as x or
    /// nodeC->set_parent<1>(*nodeA) // if result by a is passed as y
    /// ```
    /// @param parent parent node to be added
    template <std::size_t idx, typename ParentTask>
    void set_parent(ParentTask& parent)
    {
        parent.add_child(std::bind(&Node::on_argument_ready<idx>, this,
                                   std::placeholders::_1));
        parent.next_nodes.emplace_back(this);
    }

    /// @brief set_parent add a node as a prequel to current node. The parent
    /// won't be passing any result needed by current node, but it still
    /// requires the parent node job to run first before running. For example,
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// nodeA->set_parent(*nodeB);
    /// ```
    /// @param parent parent node to be added
    template <typename ParentTask>
    void set_parent(ParentTask& parent)
    {
        add_parent_count();
        SubscribeNoArgCallback cb = std::bind(
            static_cast<void (Node::*)(void)>(&Node::on_argument_ready), this);
        parent.add_child(cb);
        parent.next_nodes.emplace_back(this);
    }

    /// @brief Run the main job registered by current node
    /// After the job is finish, call the registered callback functions
    void execute() override
    {
        while (pendingCount > 0)
            ;
        if constexpr (!std::is_copy_constructible<decltype(args)>::value) {
            std::apply(job, std::move(args));
        }
        else
            std::apply(job, args);
        for (auto childTask : child_tasks) childTask();
    }

    void collect() {}

    /// @brief Add a callback function that will be called when the job in
    /// current node finished running. The object generated by the job in thise
    /// node will be passed to or consumed by the callback function `child`
    /// @param child callback function
    void add_child(SubscribeNoArgCallback child)
    {
        child_tasks.push_back(child);
    }

private:
    virtual void parent_enqueued() override { ++parentEnqueued; }
    virtual bool all_parent_enqueued() override
    {
        return parentEnqueued == parentCount;
    }
    void add_parent_count()
    {
        ++parentCount;
        ++pendingCount;
    }
    virtual void reset() override
    {
        pendingCount = parentCount;
        parentEnqueued = 0;
    }
    /// @brief A register function that can be used to register callback when
    /// parent nodes have finish running
    template <std::size_t idx>
    auto on_argument_ready(decltype(
        std::get<idx>(std::declval<std::tuple<Args...>>())) arg) -> void
    {
        if constexpr (!std::is_copy_constructible<decltype(arg)>::value) {
            std::get<idx>(args) = std::move(arg);
        }
        else
            std::get<idx>(args) = arg;
        --pendingCount;
    }

    void on_argument_ready() { --pendingCount; }
    JobCallback job;
    std::tuple<Args...> args;

    size_t parentEnqueued = 0;
    std::atomic<size_t> pendingCount =
        std::tuple_size<std::tuple<Args...>>::value;
    size_t parentCount = std::tuple_size<std::tuple<Args...>>::value;

    std::vector<SubscribeNoArgCallback> child_tasks;
};

class GraphEx {
public:
    GraphEx(size_t concurrency = 1) noexcept : pool(concurrency) {}

    /// @brief register entry point for the graph
    template <typename ReturnType, typename... Args>
    void register_input_node(Node<ReturnType, Args...>* node)
    {
        _input_nodes.emplace_back(node);
    }

    template <typename... Args>
    void register_input_node(Node<void, Args...>* node)
    {
        _input_nodes.emplace_back(node);
    }

    void register_input_node(Node<void>* node)
    {
        _input_nodes.emplace_back(node);
    }

    /// @brief check for cycle in the graph
    /// assume all the relevant nodes can be checked from input nodes
    bool has_cycle()
    {
        std::unordered_map<BaseNode*, bool> col;
        std::function<bool(BaseNode*)> dfs =
            [&](BaseNode* current_node) -> bool {
            col[current_node] = true;
            for (auto& next_node : current_node->next_nodes) {
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

    void reset()
    {
        std::unordered_set<BaseNode*> vis;
        std::function<bool(BaseNode*)> dfs =
            [&](BaseNode* current_node) -> bool {
            vis.insert(current_node);
            current_node->reset();
            for (auto& next_node : current_node->next_nodes) {
                if (vis.find(next_node) == vis.end()) {
                    dfs(next_node);
                }
            }
            return false;
        };
        for (auto& input_node : _input_nodes) {
            if (vis.find(input_node) == vis.end()) dfs(input_node);
        }
    }

    /// @brief run the graph execution from input nodes
    void execute()
    {
        // create thread pool with 4 worker threads
        std::unordered_set<BaseNode*> processed;
        std::queue<BaseNode*> qu;

        for (auto& v : _input_nodes) {
            qu.emplace(v);
            processed.emplace(v);
            for (auto& k : v->next_nodes) k->parent_enqueued();
        }

        while (!qu.empty()) {
            decltype(auto) nxt = qu.front();
            qu.pop();
            pool.push(std::bind(&BaseNode::execute, nxt));
            for (auto& next_node : nxt->next_nodes) {
                if (processed.find(next_node) == processed.end() &&
                    next_node->all_parent_enqueued()) {
                    qu.emplace(next_node);
                    processed.emplace(next_node);
                    for (auto& v : next_node->next_nodes) v->parent_enqueued();
                }
            }
        }
        pool.stop(true);
    }

private:
    ctpl::thread_pool pool;
    std::vector<BaseNode*> _input_nodes;
};

template <typename ReturnType, typename... Args>
decltype(auto) make_node(std::function<ReturnType(Args...)> func)
{
    return Node<ReturnType, Args...>(func);
}

template <typename... Args>
decltype(auto) make_node(std::function<void(Args...)> func)
{
    return Node<void, Args...>(func);
}

template <typename ReturnType>
decltype(auto) make_node(std::function<ReturnType(void)> func)
{
    return Node<ReturnType>(func);
}

decltype(auto) make_node(std::function<void(void)> func)
{
    return Node<void>(func);
}

}  // namespace GE

#endif
