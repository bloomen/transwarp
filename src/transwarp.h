// transwarp is a header-only C++ library for task concurrency
// Version: 1.2.2
// Repository: https://github.com/bloomen/transwarp
// Copyright: 2018 Christian Blume
// License: http://www.opensource.org/licenses/mit-license.php
#pragma once
#include <future>
#include <type_traits>
#include <memory>
#include <tuple>
#include <string>
#include <cstddef>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <atomic>
#include <chrono>


namespace transwarp {


// The possible task types
enum class task_type {
    root,        // The task has no parents
    consume,     // The task's functor consumes all parent results
    consume_any, // The task's functor consumes the first parent result that becomes ready
    wait,        // The task's functor takes no arguments but waits for all parents to finish
    wait_any,    // The task's functor takes no arguments but waits for the first parent to finish
};

// String conversion for the task_type enumeration
inline std::string to_string(const transwarp::task_type& type) {
    switch (type) {
    case transwarp::task_type::root: return "root";
    case transwarp::task_type::consume: return "consume";
    case transwarp::task_type::consume_any: return "consume_any";
    case transwarp::task_type::wait: return "wait";
    case transwarp::task_type::wait_any: return "wait_any";
    default: return "unknown";
    }
}


// The root type. Used for tag dispatch
struct root_type : std::integral_constant<transwarp::task_type, transwarp::task_type::root> {};
constexpr transwarp::root_type root{};

// The consume type. Used for tag dispatch
struct consume_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume> {};
constexpr transwarp::consume_type consume{};

// The consume_any type. Used for tag dispatch
struct consume_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume_any> {};
constexpr transwarp::consume_any_type consume_any{};

// The wait type. Used for tag dispatch
struct wait_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait> {};
constexpr transwarp::wait_type wait{};

// The wait_any type. Used for tag dispatch
struct wait_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait_any> {};
constexpr transwarp::wait_any_type wait_any{};


namespace detail {

struct visit;
struct unvisit;
struct final_visitor;
struct schedule_visitor;
struct node_manip;
template<bool>
struct assign_node_if_impl;

} // detail


// A node carrying meta-data of a task
class node {
public:

    node() = default;

    // delete copy/move semantics
    node(const node&) = delete;
    node& operator=(const node&) = delete;
    node(node&&) = delete;
    node& operator=(node&&) = delete;

    // The task ID
    std::size_t get_id() const noexcept {
        return id_;
    }

    // The task type
    transwarp::task_type get_type() const noexcept {
        return type_;
    }

    // The optional task name (may be null)
    const std::shared_ptr<std::string>& get_name() const noexcept {
        return name_;
    }

    // The optional, task-specific executor (may be null)
    const std::shared_ptr<std::string>& get_executor() const noexcept {
        return executor_;
    }

    // The task's parents (may be empty)
    const std::vector<std::shared_ptr<node>>& get_parents() const noexcept {
        return parents_;
    }

    // The task priority (defaults to 0)
    std::size_t get_priority() const noexcept {
        return priority_;
    }

    // The custom task data (may be null)
    const std::shared_ptr<void>& get_custom_data() const noexcept {
        return custom_data_;
    }

    // Returns whether the associated task is canceled
    bool is_canceled() const noexcept {
        return canceled_.load();
    }

private:
    friend struct transwarp::detail::node_manip;

    std::size_t id_ = 0;
    transwarp::task_type type_ = transwarp::task_type::root;
    std::shared_ptr<std::string> name_;
    std::shared_ptr<std::string> executor_;
    std::vector<std::shared_ptr<node>> parents_;
    std::size_t priority_ = 0;
    std::shared_ptr<void> custom_data_;
    std::atomic_bool canceled_{false};
};

// String conversion for the node class
inline std::string to_string(const transwarp::node& node, const std::string& seperator="\n") {
    std::string s;
    s += '"';
    const auto& name = node.get_name();
    if (name) {
        s += "<" + *name + ">" + seperator;
    }
    s += transwarp::to_string(node.get_type());
    s += " id=" + std::to_string(node.get_id());
    s += " par=" + std::to_string(node.get_parents().size());
    const auto& exec = node.get_executor();
    if (exec) {
        s += seperator + "<" + *exec + ">";
    }
    s += '"';
    return s;
}


// An edge between two nodes
class edge {
public:
    // cppcheck-suppress passedByValue
    edge(std::shared_ptr<transwarp::node> parent, std::shared_ptr<transwarp::node> child) noexcept
    : parent_(std::move(parent)), child_(std::move(child))
    {}

    // default copy/move semantics
    edge(const edge&) = default;
    edge& operator=(const edge&) = default;
    edge(edge&&) = default;
    edge& operator=(edge&&) = default;

    // Returns the parent node
    const std::shared_ptr<transwarp::node>& get_parent() const noexcept {
        return parent_;
    }

    // Returns the child node
    const std::shared_ptr<transwarp::node>& get_child() const noexcept {
        return child_;
    }

private:
    std::shared_ptr<transwarp::node> parent_;
    std::shared_ptr<transwarp::node> child_;
};

// String conversion for the edge class
inline std::string to_string(const transwarp::edge& edge, const std::string& separator="\n") {
    return transwarp::to_string(*edge.get_parent(), separator) + " -> " + transwarp::to_string(*edge.get_child(), separator);
}


// Creates a dot-style string from the given graph
inline std::string to_string(const std::vector<transwarp::edge>& graph, const std::string& separator="\n") {
    std::string dot = "digraph {" + separator;
    for (const auto& edge : graph) {
        dot += transwarp::to_string(edge, separator) + separator;
    }
    dot += "}";
    return dot;
}


// The executor interface
class executor {
public:
    virtual ~executor() = default;

    // Is supposed to return the name of the executor
    virtual std::string get_name() const = 0;

    // Is supposed to run a task which is wrapped by the functor. The functor only
    // captures a shared_ptr and can hence be copied at low cost. node represents
    // the task that the functor belongs to.
    virtual void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>& node) = 0;
};


// An interface for the task class
class itask {
public:
    virtual ~itask() = default;

    virtual void set_executor(std::shared_ptr<transwarp::executor> executor) = 0;
    virtual void set_executor_all(std::shared_ptr<transwarp::executor> executor) = 0;
    virtual void remove_executor() = 0;
    virtual void remove_executor_all() = 0;
    virtual void set_priority(std::size_t priority) = 0;
    virtual void set_priority_all(std::size_t priority) = 0;
    virtual void reset_priority() = 0;
    virtual void reset_priority_all() = 0;
    virtual void set_custom_data(std::shared_ptr<void> custom_data) = 0;
    virtual void set_custom_data_all(std::shared_ptr<void> custom_data) = 0;
    virtual void remove_custom_data() = 0;
    virtual void remove_custom_data_all() = 0;
    virtual const std::shared_ptr<transwarp::node>& get_node() const noexcept = 0;
    virtual void schedule(bool reset=true) = 0;
    virtual void schedule(transwarp::executor& executor, bool reset=true) = 0;
    virtual void schedule_all(bool reset_all=true) = 0;
    virtual void schedule_all(transwarp::executor& executor, bool reset_all=true) = 0;
    virtual bool was_scheduled() const noexcept = 0;
    virtual void wait() const = 0;
    virtual bool is_ready() const = 0;
    virtual void reset() = 0;
    virtual void reset_all() = 0;
    virtual void cancel(bool enabled) noexcept = 0;
    virtual void cancel_all(bool enabled) noexcept = 0;
    virtual std::vector<transwarp::edge> get_graph() const = 0;

private:
    friend struct transwarp::detail::visit;
    friend struct transwarp::detail::unvisit;
    friend struct transwarp::detail::final_visitor;
    friend struct transwarp::detail::schedule_visitor;

    virtual void visit(const std::function<void(itask&)>& visitor) = 0;
    virtual void unvisit() noexcept = 0;
    virtual void set_node_id(std::size_t id) noexcept = 0;
    virtual void schedule_impl(bool reset, transwarp::executor* executor=nullptr) = 0;
};


namespace detail {

template<typename ResultType, bool is_void>
struct rinfo_impl;

template<typename ResultType>
struct rinfo_impl<ResultType, true> {
    using type = void;
};

template<typename ResultType>
struct rinfo_impl<ResultType, false> {
    using type = const ResultType&;
};

template<typename ResultType>
struct rinfo {
    using type = typename rinfo_impl<ResultType, std::is_void<ResultType>::value>::type;
};

} // detail


// The task class which is implemented by task_impl
template<typename ResultType>
class task : public transwarp::itask {
public:
    using result_type = ResultType;

    virtual ~task() = default;

    virtual const std::shared_future<ResultType>& get_future() const noexcept = 0;
    virtual typename transwarp::detail::rinfo<ResultType>::type get() const = 0;
};


// Base class for exceptions
class transwarp_error : public std::runtime_error {
public:
    explicit transwarp_error(const std::string& message)
    : std::runtime_error(message)
    {}
};


// Exception thrown when a task is canceled
class task_canceled : public transwarp::transwarp_error {
public:
    explicit task_canceled(const std::string& node_repr)
    : transwarp::transwarp_error("task canceled: " + node_repr)
    {}
};


// Exception thrown when a task was destroyed prematurely
class task_destroyed : public transwarp::transwarp_error {
public:
    explicit task_destroyed(const std::string& node_repr)
    : transwarp::transwarp_error("task destroyed: " + node_repr)
    {}
};


// An exception for errors in the thread_pool class
class thread_pool_error : public transwarp::transwarp_error {
public:
    explicit thread_pool_error(const std::string& message)
    : transwarp::transwarp_error(message)
    {}
};


// A base class for a user-defined functor that needs access to the node associated
// to the task or a cancel point to stop a task while it's running
class functor {
public:

    virtual ~functor() = default;

protected:

    // The node associated to the task
    const std::shared_ptr<transwarp::node>& transwarp_node() const noexcept {
        return transwarp_node_;
    }

    // If the associated task is canceled then this will throw transwarp::task_canceled
    // which will stop the task while it's running
    void transwarp_cancel_point() const {
        if (transwarp_node_->is_canceled()) {
            throw transwarp::task_canceled(std::to_string(transwarp_node_->get_id()));
        }
    }

private:
    template<bool>
    friend struct transwarp::detail::assign_node_if_impl;

    std::shared_ptr<transwarp::node> transwarp_node_;
};


namespace detail {


// Node manipulation
struct node_manip {

    static void set_type(transwarp::node& node, transwarp::task_type type) noexcept {
        node.type_ = type;
    }

    static void set_name(transwarp::node& node, std::shared_ptr<std::string> name) noexcept {
        node.name_ = name;
    }

    static void set_id(transwarp::node& node, std::size_t id) noexcept {
        node.id_ = id;
    }

    static void add_parent(transwarp::node& node, std::shared_ptr<transwarp::node> parent) {
        node.parents_.push_back(std::move(parent));
    }

    static void set_executor(transwarp::node& node, std::shared_ptr<std::string> executor) noexcept {
        if (executor) {
            node.executor_ = std::move(executor);
        } else {
            node.executor_.reset();
        }
    }

    static void set_priority(transwarp::node& node, std::size_t priority) noexcept {
        node.priority_ = priority;
    }

    static void set_custom_data(transwarp::node& node, std::shared_ptr<void> custom_data) {
        if (custom_data) {
            node.custom_data_ = std::move(custom_data);
        } else {
            node.custom_data_.reset();
        }
    }

    static void set_canceled(transwarp::node& node, bool enabled) noexcept {
        node.canceled_ = enabled;
    }

};


// A simple thread pool used to execute tasks in parallel
class thread_pool {
public:

    explicit thread_pool(std::size_t n_threads)
    : done_(false)
    {
        if (n_threads == 0) {
            throw transwarp::thread_pool_error("number of threads must be larger than zero");
        }
        const auto n_target = threads_.size() + n_threads;
        while (threads_.size() < n_target) {
            std::thread thread;
            try {
                thread = std::thread(&thread_pool::worker, this);
            } catch (...) {
                shutdown();
                throw;
            }
            try {
                threads_.push_back(std::move(thread));
            } catch (...) {
                shutdown();
                thread.join();
                throw;
            }
        }
    }

    // delete copy/move semantics
    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    ~thread_pool() {
        shutdown();
    }

    void push(const std::function<void()>& functor) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            functors_.push(functor);
        }
        cond_var_.notify_one();
    }

private:

    void worker() {
        for (;;) {
            std::function<void()> functor;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_var_.wait(lock, [this]{
                    return done_ || !functors_.empty();
                });
                if (done_ && functors_.empty()) {
                    break;
                }
                functor = functors_.front();
                functors_.pop();
            }
            functor();
        }
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
        }
        cond_var_.notify_all();
        for (auto& thread : threads_) {
            thread.join();
        }
        threads_.clear();
    }

    bool done_;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> functors_;
    std::condition_variable cond_var_;
    std::mutex mutex_;
};


template<typename Result, typename Task, typename... Args>
Result run_task(std::size_t node_id, const Task& task, Args&&... args) {
    auto t = task.lock();
    if (!t) {
        throw transwarp::task_destroyed(std::to_string(node_id));
    }
    if (t->node_->is_canceled()) {
        throw transwarp::task_canceled(std::to_string(node_id));
    }
    return t->functor_(std::forward<Args>(args)...);
}


template<typename TaskType, bool done, int total, int... n>
struct call_with_futures_impl {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple& futures) {
        return call_with_futures_impl<TaskType, total == 1 + static_cast<int>(sizeof...(n)), total, n..., static_cast<int>(sizeof...(n))>::template
                work<Result>(node_id, task, futures);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::root_type, true, total, n...> {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple&) {
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_type, true, total, n...> {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple& futures) {
        return transwarp::detail::run_task<Result>(node_id, task, std::get<n>(futures).get()...);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_any_type, true, total, n...> {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple& futures) {
        using future_t = typename std::remove_reference<decltype(std::get<0>(futures))>::type; // use first type as reference
        for (;;) {
            bool ready = false;
            auto future = waiter<future_t>::template wait(ready, std::get<n>(futures)...);
            if (ready) {
                return transwarp::detail::run_task<Result>(node_id, task, future.get());
            }
        }
    }

    template<typename Future>
    struct waiter {
        template<typename T, typename... Args>
        static Future wait(bool& ready, const T& arg, const Args& ...args) {
            const auto status = arg.wait_for(std::chrono::microseconds(1));
            if (status == std::future_status::ready) {
                ready = true;
                return arg;
            }
            return wait(ready, args...);
        }
        static Future wait(bool&) {
            return {};
        }
    };
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::wait_type, true, total, n...> {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple& futures) {
        wait(std::get<n>(futures)...);
        return transwarp::detail::run_task<Result>(node_id, task);
    }
    template<typename T, typename... Args>
    static void wait(const T& arg, const Args& ...args) {
        arg.get();
        wait(args...);
    }
    static void wait() {}
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::wait_any_type, true, total, n...> {
    template<typename Result, typename Task, typename Tuple>
    static Result work(std::size_t node_id, const Task& task, const Tuple& futures) {
        while (!wait(std::get<n>(futures)...));
        return transwarp::detail::run_task<Result>(node_id, task);
    }
    template<typename T, typename... Args>
    static bool wait(const T& arg, const Args& ...args) {
        const auto status = arg.wait_for(std::chrono::microseconds(1));
        if (status == std::future_status::ready) {
            arg.get();
            return true;
        }
        return wait(args...);
    }
    static bool wait() {
        return false;
    }
};

// Calls the functor of the given task with the results from the futures.
// Throws transwarp::task_canceled if the task is canceled.
// Throws transwarp::task_destroyed in case the task was destroyed prematurely.
template<typename TaskType, typename Result, typename Task, typename Tuple>
Result call_with_futures(std::size_t node_id, const Task& task, const Tuple& futures) {
    constexpr std::size_t n = std::tuple_size<Tuple>::value;
    return transwarp::detail::call_with_futures_impl<TaskType, 0 == n, static_cast<int>(n)>::template
            work<Result>(node_id, task, futures);
}

template<std::size_t...> struct indices {};

template<std::size_t...> struct construct_range;

template<std::size_t end, std::size_t idx, std::size_t... i>
struct construct_range<end, idx, i...> : construct_range<end, idx + 1, i..., idx> {};

template<std::size_t end, std::size_t... i>
struct construct_range<end, end, i...> {
    using type = transwarp::detail::indices<i...>;
};

template<std::size_t b, std::size_t e>
struct index_range {
    using type = typename transwarp::detail::construct_range<e, b>::type;
};

template<typename Functor, typename Tuple>
void call_with_each_index(transwarp::detail::indices<>, const Functor&, const Tuple&) {}

template<std::size_t i, std::size_t... j, typename Functor, typename Tuple>
void call_with_each_index(transwarp::detail::indices<i, j...>, const Functor& f, const Tuple& t) {
    auto ptr = std::get<i>(t);
    if (!ptr) {
        throw transwarp::transwarp_error("Not a valid pointer to a task");
    }
    f(*ptr);
    transwarp::detail::call_with_each_index(transwarp::detail::indices<j...>(), f, t);
}

// Calls the functor with every element in the tuple. Expects the tuple to contain
// task pointers only and dereferences each element before passing it into the functor
template<typename Functor, typename Tuple>
void call_with_each(const Functor& f, const Tuple& t) {
    constexpr std::size_t n = std::tuple_size<Tuple>::value;
    using index_t = typename transwarp::detail::index_range<0, n>::type;
    transwarp::detail::call_with_each_index(index_t(), f, t);
}

template<int offset, typename... ParentResults>
struct assign_futures_impl {
    static void work(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& source, std::tuple<std::shared_future<ParentResults>...>& target) {
        std::get<offset>(target) = std::get<offset>(source)->get_future();
        assign_futures_impl<offset - 1, ParentResults...>::work(source, target);
    }
};

template<typename... ParentResults>
struct assign_futures_impl<-1, ParentResults...> {
    static void work(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>&, std::tuple<std::shared_future<ParentResults>...>&) {}
};

// Returns the futures from the given tasks
template<typename... ParentResults>
std::tuple<std::shared_future<ParentResults>...> get_futures(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& input) {
    std::tuple<std::shared_future<ParentResults>...> result;
    assign_futures_impl<static_cast<int>(sizeof...(ParentResults)) - 1, ParentResults...>::work(input, result);
    return result;
}

// Sets parents of the node
struct parent_visitor {
    explicit parent_visitor(std::shared_ptr<transwarp::node>& node) noexcept
    : node_(node) {}

    void operator()(const transwarp::itask& task) const {
        transwarp::detail::node_manip::add_parent(*node_, task.get_node());
    }

    std::shared_ptr<transwarp::node>& node_;
};

// Applies final bookkeeping to the task
struct final_visitor {
    final_visitor() noexcept
    : id_(0) {}

    void operator()(transwarp::itask& task) noexcept {
        task.set_node_id(id_++);
    }

    std::size_t id_;
};

// Generates a graph
struct graph_visitor {
    explicit graph_visitor(std::vector<transwarp::edge>& graph) noexcept
    : graph_(graph) {}

    void operator()(const transwarp::itask& task) {
        const auto& node = task.get_node();
        for (const auto& parent : node->get_parents()) {
            graph_.emplace_back(parent, node);
        }
    }

    std::vector<transwarp::edge>& graph_;
};

// Schedules using the given executor
struct schedule_visitor {
    schedule_visitor(bool reset, transwarp::executor* executor) noexcept
    : reset_(reset), executor_(executor) {}

    void operator()(transwarp::itask& task) {
        task.schedule_impl(reset_, executor_);
    }

    bool reset_;
    transwarp::executor* executor_;
};

// Resets the given task
struct reset_visitor {

    template<typename Task>
    void operator()(Task& task) const {
        task.reset();
    }
};

// Cancels or resumes the given task
struct cancel_visitor {
    explicit cancel_visitor(bool enabled) noexcept
    : enabled_(enabled) {}

    void operator()(transwarp::itask& task) const noexcept {
        task.cancel(enabled_);
    }

    bool enabled_;
};

// Assigns an executor to the given task
struct set_executor_visitor {
    explicit set_executor_visitor(std::shared_ptr<transwarp::executor> executor) noexcept
    : executor_(std::move(executor)) {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_executor(executor_);
    }

    std::shared_ptr<transwarp::executor> executor_;
};

// Removes the executor from the given task
struct remove_executor_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.remove_executor();
    }
};

// Assigns a priority to the given task
struct set_priority_visitor {
    explicit set_priority_visitor(std::size_t priority) noexcept
    : priority_(priority) {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_priority(priority_);
    }

    std::size_t priority_;
};

// Resets the priority of the given task
struct reset_priority_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.reset_priority();
    }
};

// Assigns custom data to the given task
struct set_custom_data_visitor {
    explicit set_custom_data_visitor(std::shared_ptr<void> custom_data) noexcept
    : custom_data_(std::move(custom_data)) {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_custom_data(custom_data_);
    }

    std::shared_ptr<void> custom_data_;
};

// Removes custom data from the given task
struct remove_custom_data_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.remove_custom_data();
    }
};

// Visits the given task using the visitor given in the constructor
struct visit {
    explicit visit(const std::function<void(transwarp::itask&)>& visitor) noexcept
    : visitor_(visitor) {}

    void operator()(transwarp::itask& task) const {
        task.visit(visitor_);
    }

    const std::function<void(transwarp::itask&)>& visitor_;
};

// Unvisits the given task
struct unvisit {

    void operator()(transwarp::itask& task) const noexcept {
        task.unvisit();
    }
};

// Determines the result type of the Functor dispatching on the task type
template<typename TaskType, typename Functor, typename... ParentResults>
struct result {
    static_assert(std::is_same<TaskType, transwarp::root_type>::value ||
                  std::is_same<TaskType, transwarp::consume_type>::value ||
                  std::is_same<TaskType, transwarp::consume_any_type>::value ||
                  std::is_same<TaskType, transwarp::wait_type>::value ||
                  std::is_same<TaskType, transwarp::wait_any_type>::value,
                  "Invalid task type, must be one of: root, consume, consume_any, wait, wait_any");
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::root_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) == 0, "A root task cannot have parent tasks");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::consume_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A consume task must have at least one parent");
    using type = decltype(std::declval<Functor>()(std::declval<ParentResults>()...));
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::consume_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A consume_any task must have at least one parent");
    using arg_t = typename std::tuple_element<0, std::tuple<ParentResults...>>::type; // using first type as reference
    using type = decltype(std::declval<Functor>()(std::declval<arg_t>()));
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::wait_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A wait task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::wait_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A wait_any task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

template<bool is_transwarp_functor>
struct assign_node_if_impl;

template<>
struct assign_node_if_impl<true> {
    template<typename Functor>
    void operator()(Functor& functor, std::shared_ptr<transwarp::node> node) const noexcept {
        functor.transwarp_node_ = std::move(node);
    }
};

template<>
struct assign_node_if_impl<false> {
    template<typename Functor>
    void operator()(Functor&, std::shared_ptr<transwarp::node>) const noexcept {}
};

// Assigns the node to the given functor if the functor is a subclass of transwarp::functor
template<typename Functor>
void assign_node_if(Functor& functor, std::shared_ptr<transwarp::node> node) {
    transwarp::detail::assign_node_if_impl<std::is_base_of<transwarp::functor, Functor>::value>{}(functor, std::move(node));
}

} // detail


// Executor for sequential execution. Runs functors sequentially on the same thread
class sequential : public transwarp::executor {
public:

    sequential() = default;

    // delete copy/move semantics
    sequential(const sequential&) = delete;
    sequential& operator=(const sequential&) = delete;
    sequential(sequential&&) = delete;
    sequential& operator=(sequential&&) = delete;

    // Returns the name of the executor
    std::string get_name() const override {
        return "transwarp::sequential";
    }

    // Runs the functor on the current thread
    void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>&) override {
        functor();
    }
};


// Executor for parallel execution. Uses a simple thread pool
class parallel : public transwarp::executor {
public:

    explicit parallel(std::size_t n_threads)
    : pool_(n_threads)
    {}

    // delete copy/move semantics
    parallel(const parallel&) = delete;
    parallel& operator=(const parallel&) = delete;
    parallel(parallel&&) = delete;
    parallel& operator=(parallel&&) = delete;

    // Returns the name of the executor
    std::string get_name() const override {
        return "transwarp::parallel";
    }

    // Pushes the functor into the thread pool for asynchronous execution
    void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>&) override {
        pool_.push(functor);
    }

private:
    transwarp::detail::thread_pool pool_;
};


// A task representing a piece of work given by functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Tasks should be created using the make_task factory functions.
template<typename TaskType, typename Functor, typename... ParentResults>
class task_impl : public transwarp::task<typename transwarp::detail::result<TaskType, Functor, ParentResults...>::type>,
                  public std::enable_shared_from_this<task_impl<TaskType, Functor, ParentResults...>> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = typename transwarp::detail::result<task_type, Functor, ParentResults...>::type;

    // A task is defined by name, functor, and parent tasks. name is optional, see overload
    // Note: A task must be created using shared_ptr (because of shared_from_this)
    template<typename F>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    task_impl(std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : task_impl(true, std::move(name), std::forward<F>(functor), std::move(parents)...)
    {}

    // This overload is for omitting the task name
    // Note: A task must be created using shared_ptr (because of shared_from_this)
    template<typename F>
    // cppcheck-suppress uninitMemberVar
    explicit task_impl(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : task_impl(false, "", std::forward<F>(functor), std::move(parents)...)
    {}

    virtual ~task_impl() = default;

    // delete copy/move semantics
    task_impl(const task_impl&) = delete;
    task_impl& operator=(const task_impl&) = delete;
    task_impl(task_impl&&) = delete;
    task_impl& operator=(task_impl&&) = delete;

    // Assigns an executor to this task which takes precedence over
    // the executor provided in schedule() or schedule_all()
    void set_executor(std::shared_ptr<transwarp::executor> executor) override {
        ensure_task_not_running();
        if (!executor) {
            throw transwarp::transwarp_error("Not a valid pointer to executor");
        }
        executor_ = std::move(executor);
        transwarp::detail::node_manip::set_executor(*node_, std::make_shared<std::string>(executor_->get_name()));
    }

    // Assigns an executor to all tasks which takes precedence over
    // the executor provided in schedule() or schedule_all()
    void set_executor_all(std::shared_ptr<transwarp::executor> executor) override {
        ensure_task_not_running();
        transwarp::detail::set_executor_visitor visitor(std::move(executor));
        visit(visitor);
        unvisit();
    }

    // Removes the executor from this task
    void remove_executor() override {
        ensure_task_not_running();
        executor_.reset();
        transwarp::detail::node_manip::set_executor(*node_, nullptr);
    }

    // Removes the executor from all tasks
    void remove_executor_all() override {
        ensure_task_not_running();
        transwarp::detail::remove_executor_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // Sets a task priority (defaults to 0). transwarp will not directly use this.
    // This is only useful if something else is using the priority (e.g. a custom executor)
    void set_priority(std::size_t priority) override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_priority(*node_, priority);
    }

    // Sets a priority to all tasks (defaults to 0). transwarp will not directly use this.
    // This is only useful if something else is using the priority (e.g. a custom executor)
    void set_priority_all(std::size_t priority) override {
        ensure_task_not_running();
        transwarp::detail::set_priority_visitor visitor(priority);
        visit(visitor);
        unvisit();
    }

    // Resets the task priority to 0
    void reset_priority() override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_priority(*node_, 0);
    }

    // Resets the priority of all tasks to 0
    void reset_priority_all() override {
        ensure_task_not_running();
        transwarp::detail::reset_priority_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // Assigns custom data to this task. transwarp will not directly use this.
    // This is only useful if something else is using this custom data (e.g. a custom executor)
    void set_custom_data(std::shared_ptr<void> custom_data) override {
        ensure_task_not_running();
        if (!custom_data) {
            throw transwarp::transwarp_error("Not a valid pointer to custom data");
        }
        transwarp::detail::node_manip::set_custom_data(*node_, std::move(custom_data));
    }

    // Assigns custom data to all tasks. transwarp will not directly use this.
    // This is only useful if something else is using this custom data (e.g. a custom executor)
    void set_custom_data_all(std::shared_ptr<void> custom_data) override {
        ensure_task_not_running();
        transwarp::detail::set_custom_data_visitor visitor(std::move(custom_data));
        visit(visitor);
        unvisit();
    }

    // Removes custom data from this task
    void remove_custom_data() override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_custom_data(*node_, nullptr);
    }

    // Removes custom data from all tasks
    void remove_custom_data_all() override {
        ensure_task_not_running();
        transwarp::detail::remove_custom_data_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // Returns the future associated to the underlying execution
    const std::shared_future<result_type>& get_future() const noexcept override {
        return future_;
    }

    // Returns the associated node
    const std::shared_ptr<transwarp::node>& get_node() const noexcept override {
        return node_;
    }

    // Schedules this task for execution on the caller thread.
    // The task-specific executor gets precedence if it exists.
    // reset denotes whether schedule should reset the underlying
    // future and schedule even if the future is already valid.
    void schedule(bool reset=true) override {
        ensure_task_not_running();
        schedule_impl(reset);
    }

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    // reset denotes whether schedule should reset the underlying
    // future and schedule even if the future is already valid.
    void schedule(transwarp::executor& executor, bool reset=true) override {
        ensure_task_not_running();
        schedule_impl(reset, &executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(bool reset_all=true) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all);
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(transwarp::executor& executor, bool reset_all=true) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, &executor);
    }

    // Returns whether the task was scheduled and not reset afterwards.
    // This means that the underlying future is valid
    bool was_scheduled() const noexcept override {
        return future_.valid();
    }

    // Waits for the task to complete. Should only be called if was_scheduled()
    // is true, throws transwarp::transwarp_error otherwise
    void wait() const override {
        ensure_task_was_scheduled();
        future_.wait();
    }

    // Returns whether the task has finished processing. Should only be called
    // if was_scheduled() is true, throws transwarp::transwarp_error otherwise
    bool is_ready() const override {
        ensure_task_was_scheduled();
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    // Returns the result of this task. Throws any exceptions that the underlying
    // functor throws. Should only be called if was_scheduled() is true,
    // throws transwarp::transwarp_error otherwise
    // Note that the return type is either 'void' or 'const result_type&'
    typename transwarp::detail::rinfo<result_type>::type get() const override {
        ensure_task_was_scheduled();
        return future_.get();
    }

    // Resets the future of this task
    void reset() override {
        ensure_task_not_running();
        future_ = std::shared_future<result_type>();
    }

    // Resets the futures of all tasks in the graph
    void reset_all() override {
        ensure_task_not_running();
        transwarp::detail::reset_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // If enabled then this task is canceled which will
    // throw transwarp::task_canceled when retrieving the task result.
    // As long as cancel is enabled new computations cannot be scheduled.
    // Passing false is equivalent to resume.
    void cancel(bool enabled) noexcept override {
        transwarp::detail::node_manip::set_canceled(*node_, enabled);
    }

    // If enabled then all pending tasks in the graph are canceled which will
    // throw transwarp::task_canceled when retrieving the task result.
    // As long as cancel is enabled new computations cannot be scheduled.
    // Passing false is equivalent to resume.
    void cancel_all(bool enabled) noexcept override {
        transwarp::detail::cancel_visitor visitor(enabled);
        visit(visitor);
        unvisit();
    }

    // Returns the graph of the task structure. This is mainly for visualizing
    // the tasks and their interdependencies. Pass the result into transwarp::to_string
    // to retrieve a dot-style graph representation for easy viewing.
    std::vector<transwarp::edge> get_graph() const override {
        std::vector<transwarp::edge> graph;
        transwarp::detail::graph_visitor visitor(graph);
        const_cast<task_impl*>(this)->visit(visitor);
        const_cast<task_impl*>(this)->unvisit();
        return graph;
    }

private:

    template<typename F>
    // cppcheck-suppress passedByValue
    task_impl(bool has_name, std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : node_(std::make_shared<transwarp::node>()),
      functor_(std::forward<F>(functor)),
      parents_(std::make_tuple(std::move(parents)...)),
      visited_(false)
    {
        transwarp::detail::node_manip::set_type(*node_, task_type::value);
        transwarp::detail::node_manip::set_name(*node_, (has_name ? std::make_shared<std::string>(std::move(name)) : nullptr));
        transwarp::detail::assign_node_if(functor_, node_);
        transwarp::detail::call_with_each(transwarp::detail::parent_visitor(node_), parents_);
        transwarp::detail::final_visitor visitor;
        visit(visitor);
        unvisit();
    }

    template<typename R, typename T, typename... A>
    friend R transwarp::detail::run_task(std::size_t, const T&, A&&...);

    // Assigns the given id to the node
    void set_node_id(std::size_t id) noexcept override {
        transwarp::detail::node_manip::set_id(*node_, id);
    }

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    // Runs the task on the same thread as the caller if neither the global
    // nor the task-specific executor is found.
    void schedule_impl(bool reset, transwarp::executor* executor=nullptr) override {
        if (!node_->is_canceled() && (reset || !future_.valid())) {
            std::weak_ptr<task_impl> self = this->shared_from_this();
            auto futures = transwarp::detail::get_futures(parents_);
            auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
                    std::bind(&task_impl::evaluate, node_->get_id(), std::move(self), std::move(futures)));
            future_ = pack_task->get_future();
            if (executor_) {
                executor_->execute([pack_task] { (*pack_task)(); }, node_);
            } else if (executor) {
                executor->execute([pack_task] { (*pack_task)(); }, node_);
            } else {
                (*pack_task)();
            }
        }
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // Runs tasks on the same thread as the caller if neither the global
    // nor a task-specific executor is found.
    void schedule_all_impl(bool reset_all, transwarp::executor* executor=nullptr) {
        if (!node_->is_canceled()) {
            transwarp::detail::schedule_visitor visitor(reset_all, executor);
            visit(visitor);
            unvisit();
        }
    }

    // Visits each task in a depth-first traversal.
    void visit(const std::function<void(transwarp::itask&)>& visitor) override {
        if (!visited_) {
            transwarp::detail::call_with_each(transwarp::detail::visit(visitor), parents_);
            visitor(*this);
            visited_ = true;
        }
    }

    // Traverses through all tasks and marks them as not visited.
    void unvisit() noexcept override {
        if (visited_) {
            visited_ = false;
            transwarp::detail::call_with_each(transwarp::detail::unvisit(), parents_);
        }
    }

    // Checks if the task is currently running and throws transwarp::transwarp_error if it is
    void ensure_task_not_running() const {
        if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            throw transwarp::transwarp_error("the task is currently running: " + std::to_string(node_->get_id()));
        }
    }

    // Checks if the task was scheduled and throws transwarp::transwarp_error if it's not
    void ensure_task_was_scheduled() const {
        if (!future_.valid()) {
            throw transwarp::transwarp_error("the task was not scheduled: " + std::to_string(node_->get_id()));
        }
    }

    // Calls the functor of the given task with the results from the futures.
    // Throws transwarp::task_canceled if the task is canceled.
    // Throws transwarp::task_destroyed in case the task was destroyed prematurely.
    static result_type evaluate(std::size_t node_id, const std::weak_ptr<task_impl>& task,
                                const std::tuple<std::shared_future<ParentResults>...>& futures) {
        return transwarp::detail::call_with_futures<task_type, result_type>(node_id, task, futures);
    }

    std::shared_ptr<transwarp::node> node_;
    Functor functor_;
    std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...> parents_;
    bool visited_;
    std::shared_ptr<transwarp::executor> executor_;
    std::shared_future<result_type> future_;
};


// A factory function to create a new task
template<typename TaskType, typename Functor, typename... Parents>
auto make_task(TaskType, std::string name, Functor&& functor, std::shared_ptr<Parents>... parents) -> decltype(std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::move(name), std::forward<Functor>(functor), std::move(parents)...)) {
    return std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::move(name), std::forward<Functor>(functor), std::move(parents)...);
}

// A factory function to create a new task. Overload for omitting the task name
template<typename TaskType, typename Functor, typename... Parents>
auto make_task(TaskType, Functor&& functor, std::shared_ptr<Parents>... parents) -> decltype(std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::forward<Functor>(functor), std::move(parents)...)) {
    return std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::forward<Functor>(functor), std::move(parents)...);
}


} // transwarp
