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
#include <list>
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

class GraphNode
{
    using Dependents = std::vector<GraphNode *>;
    using ExecuteCallback = std::function<void()>;
    using ParentEnqueuedCallback = std::function<void()>;
    using AllParentEnqueuedCallback = std::function<bool()>;
    using AddParentCountCallback = std::function<void()>;
    using ResetCallback = std::function<void()>;
public:
    template <typename NodeType>
    GraphNode(NodeType* nodePtr, uint32_t idx)
        : _idx(idx)
        , _execute(std::bind(&NodeType::execute, nodePtr))
        , _parentEnqueued(std::bind(&NodeType::parentEnqueued, nodePtr))
        , _allParentEnqueued(std::bind(&NodeType::allParentEnqueued, nodePtr))
        , _addParentCount(std::bind(&NodeType::addParentCount, nodePtr))
        , _reset(std::bind(&NodeType::reset, nodePtr))
    {}

    void registerDependent(GraphNode* dependent) {
        _dependents.push_back(dependent);
    }
    Dependents& getDependents() { return _dependents; }
    uint32_t getIndex() const { return _idx; }

    void execute() { _execute(); } // TODO: const?
    void parentEnqueued() { _parentEnqueued(); } // TODO: const?
    bool allParentEnqueued() { return _allParentEnqueued(); } // TODO: const?
    void addParentCount() { _addParentCount(); } // TODO: const?
    void reset() { _reset(); } // TODO: const?

private:
    // nodes that depend on the execution of this node
    Dependents _dependents;
    // debugging
    uint32_t _idx;

    ExecuteCallback _execute;
    ParentEnqueuedCallback _parentEnqueued;
    AllParentEnqueuedCallback _allParentEnqueued;
    AddParentCountCallback _addParentCount;
    ResetCallback _reset;
};

class GraphEx {
public:
    GraphEx(size_t concurrency = 1) noexcept : _pool(concurrency) {}

    /// @brief register entry point for the graph
    template <typename NodeType>
    void registerInputNode(NodeType* node)
    {
        // Assertion fail means:
        // Invalid NodeType, not created using GraphEx::makeNode method.
        assert(node->getGraphNode());
        _inputNodes.push_back(node->getGraphNode());
    }

    template <typename NodeType>
    GraphNode* registerGraphNode(NodeType* node)
    {
        _allNodes.emplace_back(node, _allNodes.size());
        return &_allNodes.back();
    }

    /// @brief check for cycle in the graph
    /// assume all the relevant nodes can be checked from input nodes
    bool hasCycle()
    {
        std::unordered_map<GraphNode*, bool> col;
        std::function<bool(GraphNode*)> dfs =
            [&](GraphNode* currentNode) -> bool {
            col[currentNode] = true;
            for (auto* nextNode : currentNode->getDependents()) {
                if (likely(!col.count(nextNode))) {
                    if (dfs(nextNode)) {
                        return true;
                    }
                }
                else if (unlikely(col[nextNode] == true)) {
                    return true;
                }
            }
            col[currentNode] = false;
            return false;
        };
        for (auto* inputNode : _inputNodes) {

            col.clear();
            if (dfs(inputNode)) {
                return true;
            }
        }
        return false;
    }

    void reset()
    {
        std::unordered_set<GraphNode*> vis;
        std::function<bool(GraphNode*)> dfs =
            [&](GraphNode* currentNode) -> bool {
            vis.insert(currentNode);
            currentNode->reset();
            for (auto* nextNode : currentNode->getDependents()) {
                if (vis.find(nextNode) == vis.end()) {
                    dfs(nextNode);
                }
            }
            return false;
        };
        for (auto* inputNode : _inputNodes) {
            if (vis.find(inputNode) == vis.end()) dfs(inputNode);
        }
    }

    /// @brief run the graph execution from input nodes
    void execute()
    {
        // create thread _pool with 4 worker threads
        std::unordered_set<GraphNode*> processed;
        std::queue<GraphNode*> qu;

        for (auto* v : _inputNodes) {
            qu.emplace(v);
            processed.emplace(v);
            for (auto* k : v->getDependents()) k->parentEnqueued();
        }

        while (!qu.empty()) {
            auto* nxt = qu.front();
            qu.pop();
            _pool.push(std::bind(&GraphNode::execute, nxt));
            for (auto* nextNode : nxt->getDependents()) {
                if (processed.find(nextNode) == processed.end() &&
                    nextNode->allParentEnqueued()) {
                    qu.emplace(nextNode);
                    processed.emplace(nextNode);
                    for (auto* v : nextNode->getDependents()) v->parentEnqueued();
                }
            }
        }
        _pool.stop(true);
    }

private:

    ctpl::thread_pool _pool;
    std::list<GraphNode> _allNodes; // lifetime control
    std::vector<GraphNode*> _inputNodes; // source nodes
};

template <typename TaskCallback, typename... Args>
class Node {
    friend class GraphEx;
    friend class GraphNode;
public:
    using NodeType = Node<TaskCallback, Args...>;
    using ReturnType = std::invoke_result_t<TaskCallback, Args...>;
    using ResultStorage = typename std::conditional_t<
        !std::is_void_v<ReturnType>,
        ReturnType,
        int /* placeholder type for void-returning function */
        >;
    using SubscribeCallback = std::function<
        void(typename std::conditional_t<
            !std::is_void_v<ReturnType>,
            ReturnType,
            std::false_type
            >)
        >;
    using SubscribeNoArgCallback = std::function<void(void)>;

    Node(GraphEx& executor, TaskCallback task)
        : _task(task)
        , _graphNode(executor.registerGraphNode(this))
    {}

    /// @brief setParent add a node as a prequel to current node, and the
    /// result of parent node will be passed or consumed by current node once
    /// the parent node _task is done
    /// @param idx template argument of the position of result object in current
    /// node's `_args` For example, let's say there are 3 functions, whose
    /// signatures as followed
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// c = [](int x, int y) -> int {}
    /// ```
    /// We need to know whether the result returned by a is passed as `x` or `y`
    /// to c. Using `setParent`, we can specify by
    /// ```
    /// nodeC->setParent<0>(*nodeA) // if result by a is passed as x or
    /// nodeC->setParent<1>(*nodeA) // if result by a is passed as y or
    /// ```
    /// @param parent parent node to be added
    template <std::size_t idx, typename ParentTask>
    void setParent(ParentTask& parent)
    {
        static_assert(!std::is_same_v<typename ParentTask::ReturnType, void>,
            "Could not record result of a function that returns void as "
            "an argument for this task");
        parent.addChild(
            std::bind(&NodeType::onArgumentReady<idx>, this,
                std::placeholders::_1
                )
            );

        // Assertion fail means:
        // Invalid NodeType, not created using GraphEx::makeNode method.
        assert(parent.getGraphNode() && _graphNode);
        parent.getGraphNode()->registerDependent(_graphNode);
    }

    /// @brief setParent add a node as a prequel to current node. The parent
    /// won't be passing any result needed by current node, but it still
    /// requires the parent node _task to run first before running. For example,
    /// ```
    /// a = []() -> int {}
    /// b = []() -> int {}
    /// nodeA->setParent(*nodeB);
    /// ```
    /// @param parent parent node to be added
    template <typename ParentTask>
    void setParent(ParentTask& parent)
    {
        addParentCount();
        SubscribeNoArgCallback cb = std::bind(
            static_cast<void (NodeType::*)(void)>(&NodeType::onArgumentReady),
            this
            );
        parent.addChild(cb);

        // Assertion fail means:
        // Invalid NodeType, not created using GraphEx::makeNode method.
        assert(parent.getGraphNode() && _graphNode);
        parent.getGraphNode()->registerDependent(_graphNode);
    }

    /// @brief Run the main _task registered by current node
    /// After the _task is finish, call the registered callback functions
    /// @throw if `ReturnType` is non-copyable but there are more than 1 child
    /// tasks that require the result object
    void execute() // TODO: move this to private
    {
        // wait for result to be ready
        while (_pendingCount > 0) ;
        if constexpr (std::is_void_v<ReturnType>) {
            if constexpr (!std::is_copy_constructible<decltype(_args)>::value) {
                std::apply(_task, std::move(_args));
            }
            else
                std::apply(_task, _args);
        }
        else {
            if constexpr (!std::is_copy_constructible<ReturnType>::value) {
                DE_ENFORCE(_childTasks.size() <= 1,
                        "Internal Error: More than 1 child process for "
                        "non-copyable object");
                _result = std::apply(_task, std::move(_args));
                if (!_childTasks.empty()) {
                    _childTasks[0](std::move(_result.value()));
                    _result.reset();
                }
            }
            else {
                _result = std::apply(_task, _args);
                for (size_t i = 0; i < _childTasks.size(); ++i) {
                    _childTasks[i](_result.value());
                }
            }
        }

        for (auto childTask : _noArgChildTasks) childTask();
    }

    /// @brief Retrieve the result obtained by the current node _task
    /// @throw std::runtime_error if No valid result can be retrieved in the
    /// node
    ReturnType collect()
    {
        if (!_result) {
            throw std::runtime_error("No result found in node");
        }
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            return std::move(_result.value());
        }
        else {
            return _result.value();
        }
    }

    /// @brief Add a callback function that will be called when the _task in
    /// current node finished running. The object generated by the _task in thise
    /// node will be passed to or consumed by the callback function `child`
    /// @param child callback function
    void addChild(SubscribeCallback child)
    {
        if constexpr (!std::is_copy_constructible<ReturnType>::value) {
            if (_childTasks.size() > 0) {
                throw std::logic_error(
                    "Non copyable result cannot be passed to more than 1 child "
                    "process");
            }
            if (_isOutput) {
                throw std::logic_error(
                    "Non copyable result which has been marked as output "
                    "cannot have children");
            }
        }
        _childTasks.push_back(child);
    }

    /// @brief Add a callback function that will be called when the _task in
    /// current node finished running. No argument will be passed to the
    /// callback function
    /// @param child callback function
    void addChild(SubscribeNoArgCallback child)
    {
        _noArgChildTasks.push_back(child);
    }

    /// @brief markAsOutput mark the node as output node and preserve the
    /// value returned by the _task function. If a node is not marked as output
    /// node, there is no guarantee The result retrieved from the node is valid
    void markAsOutput() { _isOutput = true; }

    /// @brief A register function that can be used to register callback when
    /// parent nodes have finish running
    template <std::size_t idx>
    void onArgumentReady(decltype(std::get<idx>(std::declval<std::tuple<Args...>>())) arg)
    {
        if constexpr (!std::is_copy_constructible<decltype(arg)>::value) {
            std::get<idx>(_args) = std::move(arg);
        }
        else
            std::get<idx>(_args) = arg;
        --_pendingCount;
    }

    void onArgumentReady() { --_pendingCount; }

    GraphNode* getGraphNode() const { return _graphNode; }
    void setGraphNode(GraphNode* graphNode) { _graphNode = graphNode; }

private:
    void parentEnqueued() { ++_parentEnqueued; }
    bool allParentEnqueued() const
    {
        return _parentEnqueued == _parentCount;
    }
    void addParentCount()
    {
        ++_parentCount;
        ++_pendingCount;
    }
    void reset()
    {
        _result.reset();
        _pendingCount = _parentCount;
        _parentEnqueued = 0;
    }

    TaskCallback _task;
    std::tuple<Args...> _args;
    std::optional<ResultStorage> _result;

    size_t _parentEnqueued = 0;
    std::atomic<size_t> _pendingCount = std::tuple_size<std::tuple<Args...>>::value;
    size_t _parentCount = std::tuple_size<std::tuple<Args...>>::value;

    std::vector<SubscribeCallback> _childTasks;
    std::vector<SubscribeNoArgCallback> _noArgChildTasks;

    // populate only when this node is registered with GraphEx.
    GraphNode* _graphNode = nullptr;

    // if a node is not marked as output node, there is no
    // guarantee result retrieved from the node is valid.
    bool _isOutput = false;
};

template <typename ReturnType, typename... Args>
decltype(auto) makeNode(GraphEx& executor, std::function<ReturnType(Args...)> func)
{
    return Node<std::function<ReturnType(Args...)>, Args...>(executor, func);
}

template <typename... Args>
decltype(auto) makeNode(GraphEx& executor, std::function<void(Args...)> func)
{
    return Node<std::function<void(Args...)>, Args...>(executor, func);
}

decltype(auto) makeNode(GraphEx& executor, std::function<void()> func)
{
    return Node<std::function<void()>>(executor, func);
}

}  // namespace GE

#endif
