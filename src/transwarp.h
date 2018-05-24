// transwarp is a header-only C++ library for task concurrency
// Version: 1.4.0-dev
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
#include <exception>


namespace transwarp {


// The possible task types
enum class task_type {
    root,        // The task has no parents
    accept,      // The task's functor accepts all parent futures
    accept_any,  // The task's functor accepts the first parent future that becomes ready
    consume,     // The task's functor consumes all parent results
    consume_any, // The task's functor consumes the first parent result that becomes ready
    wait,        // The task's functor takes no arguments but waits for all parents to finish
    wait_any,    // The task's functor takes no arguments but waits for the first parent to finish
};

// String conversion for the task_type enumeration
inline std::string to_string(const transwarp::task_type& type) {
    switch (type) {
    case transwarp::task_type::root: return "root";
    case transwarp::task_type::accept: return "accept";
    case transwarp::task_type::accept_any: return "accept_any";
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

// The accept type. Used for tag dispatch
struct accept_type : std::integral_constant<transwarp::task_type, transwarp::task_type::accept> {};
constexpr transwarp::accept_type accept{};

// The accept_any type. Used for tag dispatch
struct accept_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::accept_any> {};
constexpr transwarp::accept_any_type accept_any{};

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


// Determines in which order tasks are scheduled in the graph
enum class schedule_type {
    breadth, // Scheduling according to a breadth-first search (default)
    depth,   // Scheduling according to a depth-first search
};


namespace detail {

struct visit_depth;
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

    // The task level
    std::size_t get_level() const noexcept {
        return level_;
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
    std::size_t level_ = 0;
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
    s += " lev=" + std::to_string(node.get_level());
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


class itask;


// An enum of events used with the listener pattern
enum class event_type {
    scheduled, // just after a task is scheduled
    started,   // just before a task is run
    finished,  // just after a task finishes
};


// The listener interface
class listener {
public:
    virtual ~listener() = default;

    virtual void handle_event(transwarp::event_type event, const transwarp::itask& task) = 0;
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
    virtual void schedule() = 0;
    virtual void schedule(transwarp::executor& executor) = 0;
    virtual void schedule(bool reset) = 0;
    virtual void schedule(transwarp::executor& executor, bool reset) = 0;
    virtual void schedule_all() = 0;
    virtual void schedule_all(transwarp::executor& executor) = 0;
    virtual void schedule_all(bool reset_all) = 0;
    virtual void schedule_all(transwarp::executor& executor, bool reset_all) = 0;
    virtual void schedule_all(transwarp::schedule_type type) = 0;
    virtual void schedule_all(transwarp::executor& executor, transwarp::schedule_type type) = 0;
    virtual void schedule_all(transwarp::schedule_type type, bool reset_all) = 0;
    virtual void schedule_all(transwarp::executor& executor, transwarp::schedule_type type, bool reset_all) = 0;
    virtual void set_exception(std::exception_ptr exception) = 0;
    virtual bool was_scheduled() const noexcept = 0;
    virtual void wait() const = 0;
    virtual bool is_ready() const = 0;
    virtual bool has_result() const = 0;
    virtual void add_listener(std::shared_ptr<transwarp::listener> listener) = 0;
    virtual void remove_listener(const std::shared_ptr<transwarp::listener>& listener) = 0;
    virtual void reset() = 0;
    virtual void reset_all() = 0;
    virtual void cancel(bool enabled) noexcept = 0;
    virtual void cancel_all(bool enabled) noexcept = 0;
    virtual std::vector<transwarp::edge> get_graph() const = 0;

protected:
    virtual void schedule_impl(bool reset, transwarp::executor* executor=nullptr) = 0;

private:
    friend struct transwarp::detail::visit_depth;
    friend struct transwarp::detail::unvisit;
    friend struct transwarp::detail::final_visitor;
    friend struct transwarp::detail::schedule_visitor;

    virtual void visit_depth(const std::function<void(itask&)>& visitor) = 0;
    virtual void unvisit() noexcept = 0;
    virtual void set_node_id(std::size_t id) noexcept = 0;
};


// Removes reference and const from a type
template<typename T>
struct remove_refc {
    using type = typename std::remove_reference<typename std::remove_const<T>::type>::type;
};


// Returns the result type of a std::shared_future<T>
template<typename T>
struct result_info {
    using type = typename std::result_of<decltype(&std::shared_future<T>::get)(std::shared_future<T>)>::type;
};


// The task class (non-void result type)
template<typename ResultType>
class task : public transwarp::itask {
public:
    using result_type = ResultType;

    virtual ~task() = default;

    virtual void set_value(const typename transwarp::remove_refc<result_type>::type& value) = 0;
    virtual void set_value(typename transwarp::remove_refc<result_type>::type&& value) = 0;
    virtual const std::shared_future<result_type>& get_future() const noexcept = 0;
    virtual typename transwarp::result_info<result_type>::type get() const = 0;
};

// The task class (non-void, non-const reference result type)
template<typename ResultType>
class task<ResultType&> : public transwarp::itask {
public:
    using result_type = ResultType&;

    virtual ~task() = default;

    virtual void set_value(typename transwarp::remove_refc<result_type>::type& value) = 0;
    virtual const std::shared_future<result_type>& get_future() const noexcept = 0;
    virtual typename transwarp::result_info<result_type>::type get() const = 0;
};

// The task class (void result type)
template<>
class task<void> : public transwarp::itask {
public:
    using result_type = void;

    virtual ~task() = default;

    virtual void set_value() = 0;
    virtual const std::shared_future<result_type>& get_future() const noexcept = 0;
    virtual result_type get() const = 0;
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

    static void set_id(transwarp::node& node, std::size_t id) noexcept {
        node.id_ = id;
    }

    static void set_level(transwarp::node& node, std::size_t level) noexcept {
        node.level_ = level;
    }

    static void set_type(transwarp::node& node, transwarp::task_type type) noexcept {
        node.type_ = type;
    }

    static void set_name(transwarp::node& node, std::shared_ptr<std::string> name) noexcept {
        node.name_ = name;
    }

    static void set_executor(transwarp::node& node, std::shared_ptr<std::string> executor) noexcept {
        if (executor) {
            node.executor_ = std::move(executor);
        } else {
            node.executor_.reset();
        }
    }

    static void add_parent(transwarp::node& node, std::shared_ptr<transwarp::node> parent) {
        node.parents_.push_back(std::move(parent));
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


inline void wait_for_all() {}

// Waits for all futures to finish
template<typename Future, typename... ParentResults>
void wait_for_all(const Future& future, const std::shared_future<ParentResults>& ...futures) {
    future.wait();
    transwarp::detail::wait_for_all(futures...);
}


template<typename FutureResult>
FutureResult wait_for_any_impl() {
    return {};
}

template<typename FutureResult, typename Future, typename... ParentResults>
FutureResult wait_for_any_impl(const Future& future, const std::shared_future<ParentResults>& ...futures) {
    const auto status = future.wait_for(std::chrono::microseconds(1));
    if (status == std::future_status::ready) {
        return future;
    }
    return transwarp::detail::wait_for_any_impl<FutureResult>(futures...);
}

// Waits for the first future to finish
template<typename FutureResult, typename... ParentResults>
FutureResult wait_for_any(const std::shared_future<ParentResults>& ...futures) {
    for (;;) {
        auto future = transwarp::detail::wait_for_any_impl<FutureResult>(futures...);
        if (future.valid()) {
            return future;
        }
    }
}


template<typename TaskType, bool done, int total, int... n>
struct call_with_futures_impl {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        return call_with_futures_impl<TaskType, total == 1 + static_cast<int>(sizeof...(n)), total, n..., static_cast<int>(sizeof...(n))>::template
                work<Result>(node_id, task, futures);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::root_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>&) {
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::accept_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        transwarp::detail::wait_for_all(std::get<n>(futures)...);
        return transwarp::detail::run_task<Result>(node_id, task, std::get<n>(futures)...);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::accept_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        using future_t = typename std::remove_reference<decltype(std::get<0>(futures))>::type; // use first type as reference
        auto future = transwarp::detail::wait_for_any<future_t>(std::get<n>(futures)...);
        return transwarp::detail::run_task<Result>(node_id, task, future);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        transwarp::detail::wait_for_all(std::get<n>(futures)...);
        return transwarp::detail::run_task<Result>(node_id, task, std::get<n>(futures).get()...);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        using future_t = typename std::remove_reference<decltype(std::get<0>(futures))>::type; // use first type as reference
        auto future = transwarp::detail::wait_for_any<future_t>(std::get<n>(futures)...);
        return transwarp::detail::run_task<Result>(node_id, task, future.get());
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::wait_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        transwarp::detail::wait_for_all(std::get<n>(futures)...);
        get_all(std::get<n>(futures)...); // ensures that exceptions are propagated
        return transwarp::detail::run_task<Result>(node_id, task);
    }
    template<typename T, typename... Args>
    static void get_all(const T& arg, const Args& ...args) {
        arg.get();
        get_all(args...);
    }
    static void get_all() {}
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::wait_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
        while (!wait(std::get<n>(futures)...));
        return transwarp::detail::run_task<Result>(node_id, task);
    }
    template<typename T, typename... Args>
    static bool wait(const T& arg, const Args& ...args) {
        const auto status = arg.wait_for(std::chrono::microseconds(1));
        if (status == std::future_status::ready) {
            arg.get(); // ensures that exceptions are propagated
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
template<typename TaskType, typename Result, typename Task, typename... ParentResults>
Result call_with_futures(std::size_t node_id, const Task& task, const std::tuple<std::shared_future<ParentResults>...>& futures) {
    constexpr std::size_t n = std::tuple_size<std::tuple<std::shared_future<ParentResults>...>>::value;
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

template<typename Functor, typename... ParentResults>
void call_with_each_index(transwarp::detail::indices<>, const Functor&, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>&) {}

template<std::size_t i, std::size_t... j, typename Functor, typename... ParentResults>
void call_with_each_index(transwarp::detail::indices<i, j...>, const Functor& f, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& t) {
    auto ptr = std::get<i>(t);
    if (!ptr) {
        throw transwarp::transwarp_error("Not a valid pointer to a task");
    }
    f(*ptr);
    transwarp::detail::call_with_each_index(transwarp::detail::indices<j...>(), f, t);
}

// Calls the functor with every element in the tuple. Expects the tuple to contain
// task pointers only and dereferences each element before passing it into the functor
template<typename Functor, typename... ParentResults>
void call_with_each(const Functor& f, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& t) {
    constexpr std::size_t n = std::tuple_size<std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>>::value;
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

// Sets parents and level of the node
struct parent_visitor {
    explicit parent_visitor(std::shared_ptr<transwarp::node>& node) noexcept
    : node_(node) {}

    void operator()(const transwarp::itask& task) const {
        transwarp::detail::node_manip::add_parent(*node_, task.get_node());
        if (node_->get_level() <= task.get_node()->get_level()) {
            // A child's level is always larger than any of its parents' levels
            transwarp::detail::node_manip::set_level(*node_, task.get_node()->get_level() + 1);
        }
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

// Pushes the given task into the vector of tasks
struct push_task_visitor {
    explicit push_task_visitor(std::vector<transwarp::itask*>& tasks)
    : tasks_(tasks) {}

    void operator()(transwarp::itask& task) {
        tasks_.push_back(&task);
    }

    std::vector<transwarp::itask*>& tasks_;
};

// Visits the given task using the visitor given in the constructor
struct visit_depth {
    explicit visit_depth(const std::function<void(transwarp::itask&)>& visitor) noexcept
    : visitor_(visitor) {}

    void operator()(transwarp::itask& task) const {
        task.visit_depth(visitor_);
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
                  std::is_same<TaskType, transwarp::accept_type>::value ||
                  std::is_same<TaskType, transwarp::accept_any_type>::value ||
                  std::is_same<TaskType, transwarp::consume_type>::value ||
                  std::is_same<TaskType, transwarp::consume_any_type>::value ||
                  std::is_same<TaskType, transwarp::wait_type>::value ||
                  std::is_same<TaskType, transwarp::wait_any_type>::value,
                  "Invalid task type, must be one of: root, accept, accept_any, consume, consume_any, wait, wait_any");
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::root_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) == 0, "A root task cannot have parent tasks");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::accept_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "An accept task must have at least one parent");
    using type = decltype(std::declval<Functor>()(std::declval<std::shared_future<ParentResults>>()...));
};

template<typename Functor, typename... ParentResults>
struct result<transwarp::accept_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "An accept_any task must have at least one parent");
    using arg_t = typename std::tuple_element<0, std::tuple<ParentResults...>>::type; // using first type as reference
    using type = decltype(std::declval<Functor>()(std::declval<std::shared_future<arg_t>>()));
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
void assign_node_if(Functor& functor, std::shared_ptr<transwarp::node> node) noexcept {
    transwarp::detail::assign_node_if_impl<std::is_base_of<transwarp::functor, Functor>::value>{}(functor, std::move(node));
}

// Returns a ready future with the given value as its state
template<typename ResultType, typename Value>
std::shared_future<ResultType> make_future_with_value(Value&& value) {
    std::promise<ResultType> promise;
    promise.set_value(std::forward<Value>(value));
    return promise.get_future();
}

// Returns a ready future
inline std::shared_future<void> make_ready_future() {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
}

// Returns a ready future with the given exception as its state
template<typename ResultType>
std::shared_future<ResultType> make_future_with_exception(std::exception_ptr exception) {
    if (!exception) {
        throw transwarp::transwarp_error("invalid exception pointer");
    }
    std::promise<ResultType> promise;
    promise.set_exception(exception);
    return promise.get_future();
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


namespace detail {

// The base task class that contains the functionality that can be used
// with all result types (void and non-void) which is almost everything.
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_base : public transwarp::task<ResultType>,
                       public std::enable_shared_from_this<task_impl_base<ResultType, TaskType, Functor, ParentResults...>> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = ResultType;

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
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
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
    // This overload will reset the underlying future.
    void schedule() override {
        ensure_task_not_running();
        this->schedule_impl(true);
    }

    // Schedules this task for execution on the caller thread.
    // The task-specific executor gets precedence if it exists.
    // reset denotes whether schedule should reset the underlying
    // future and schedule even if the future is already valid.
    void schedule(bool reset) override {
        ensure_task_not_running();
        this->schedule_impl(reset);
    }

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    // This overload will reset the underlying future.
    void schedule(transwarp::executor& executor) override {
        ensure_task_not_running();
        this->schedule_impl(true, &executor);
    }

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    // reset denotes whether schedule should reset the underlying
    // future and schedule even if the future is already valid.
    void schedule(transwarp::executor& executor, bool reset) override {
        ensure_task_not_running();
        this->schedule_impl(reset, &executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    // This overload will reset the underlying futures.
    void schedule_all() override {
        ensure_task_not_running();
        schedule_all_impl(true, transwarp::schedule_type::breadth);
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // This overload will reset the underlying futures.
    void schedule_all(transwarp::executor& executor) override {
        ensure_task_not_running();
        schedule_all_impl(true, transwarp::schedule_type::breadth, &executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, transwarp::schedule_type::breadth);
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(transwarp::executor& executor, bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, transwarp::schedule_type::breadth, &executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    // This overload will reset the underlying futures.
    void schedule_all(transwarp::schedule_type type) override {
        ensure_task_not_running();
        schedule_all_impl(true, type);
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // This overload will reset the underlying futures.
    void schedule_all(transwarp::executor& executor, transwarp::schedule_type type) override {
        ensure_task_not_running();
        schedule_all_impl(true, type, &executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(transwarp::schedule_type type, bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, type);
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // reset_all denotes whether schedule_all should reset the underlying
    // futures and schedule even if the futures are already present.
    void schedule_all(transwarp::executor& executor, transwarp::schedule_type type, bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, type, &executor);
    }

    // Assigns an exception to this task. Scheduling will have no effect after an exception
    // has been set. Calling reset() will remove the exception and re-enable scheduling.
    void set_exception(std::exception_ptr exception) override {
        ensure_task_not_running();
        future_ = transwarp::detail::make_future_with_exception<result_type>(exception);
        schedule_mode_ = false;
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

    // Returns whether this task contains a result
    bool has_result() const noexcept override {
        return was_scheduled() && is_ready();
    }

    void add_listener(std::shared_ptr<transwarp::listener> listener) override {
        if (!listener) {
            throw transwarp::transwarp_error("Not a valid pointer to listener");
        }
        listeners_.push_back(std::move(listener));
    }

    void remove_listener(const std::shared_ptr<transwarp::listener>& listener) override {
        if (!listener) {
            throw transwarp::transwarp_error("Not a valid pointer to listener");
        }
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
    }

    // Resets the future of this task
    void reset() override {
        ensure_task_not_running();
        future_ = std::shared_future<result_type>();
        schedule_mode_ = true;
    }

    // Resets the futures of all tasks in the graph
    void reset_all() override {
        ensure_task_not_running();
        transwarp::detail::reset_visitor visitor;
        visit_depth_all(visitor);
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
        visit_depth_all(visitor);
    }

    // Returns the graph of the task structure. This is mainly for visualizing
    // the tasks and their interdependencies. Pass the result into transwarp::to_string
    // to retrieve a dot-style graph representation for easy viewing.
    std::vector<transwarp::edge> get_graph() const override {
        std::vector<transwarp::edge> graph;
        transwarp::detail::graph_visitor visitor(graph);
        const_cast<task_impl_base*>(this)->visit_depth_all(visitor);
        return graph;
    }

protected:

    template<typename F>
    // cppcheck-suppress passedByValue
    task_impl_base(bool has_name, std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : node_(std::make_shared<transwarp::node>()),
      functor_(std::forward<F>(functor)),
      parents_(std::make_tuple(std::move(parents)...))
    {
        transwarp::detail::node_manip::set_type(*node_, task_type::value);
        transwarp::detail::node_manip::set_name(*node_, (has_name ? std::make_shared<std::string>(std::move(name)) : nullptr));
        transwarp::detail::assign_node_if(functor_, node_);
        transwarp::detail::call_with_each(transwarp::detail::parent_visitor(node_), parents_);
        transwarp::detail::final_visitor visitor;
        visit_depth(visitor);
        unvisit();
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

    bool schedule_mode_ = true;
    std::shared_future<result_type> future_;

private:

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
        if (schedule_mode_ && !node_->is_canceled() && (reset || !future_.valid())) {
            std::weak_ptr<task_impl_base> self = this->shared_from_this();
            auto futures = transwarp::detail::get_futures(parents_);
            auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
                    std::bind(&transwarp::detail::call_with_futures<
                              task_type, result_type, std::weak_ptr<task_impl_base>, ParentResults...>,
                              node_->get_id(), std::move(self), std::move(futures)));
            future_ = pack_task->get_future();
            auto callable = [pack_task,self] {
                {
                    auto t = self.lock();
                    if (t) {
                        t->raise_event(transwarp::event_type::started);
                    }
                }
                (*pack_task)();
                {
                    auto t = self.lock();
                    if (t) {
                        t->raise_event(transwarp::event_type::finished);
                    }
                }
            };
            raise_event(transwarp::event_type::scheduled);
            if (executor_) {
                executor_->execute(callable, node_);
            } else if (executor) {
                executor->execute(callable, node_);
            } else {
                callable();
            }
        }
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    // Runs tasks on the same thread as the caller if neither the global
    // nor a task-specific executor is found.
    void schedule_all_impl(bool reset_all, transwarp::schedule_type type, transwarp::executor* executor=nullptr) {
        if (!node_->is_canceled()) {
            transwarp::detail::schedule_visitor visitor(reset_all, executor);
            switch (type) {
            case transwarp::schedule_type::breadth:
                visit_breadth_all(visitor);
                break;
            case transwarp::schedule_type::depth:
                visit_depth_all(visitor);
                break;
            default:
                throw transwarp::transwarp_error("No such schedule type");
            }
        }
    }

    // Visits all tasks in a breadth-first traversal.
    template<typename Visitor>
    void visit_breadth_all(Visitor& visitor) {
        if (breadth_tasks_.empty()) {
            breadth_tasks_.reserve(node_->get_id() + 1);
            visit_depth(transwarp::detail::push_task_visitor(breadth_tasks_));
            unvisit();
            auto compare = [](const transwarp::itask* const l, const transwarp::itask* const r) {
                const auto l_level = l->get_node()->get_level();
                const auto l_id = l->get_node()->get_id();
                const auto r_level = r->get_node()->get_level();
                const auto r_id = r->get_node()->get_id();
                return std::tie(l_level, l_id) < std::tie(r_level, r_id);
            };
            std::sort(breadth_tasks_.begin(), breadth_tasks_.end(), compare);
        }
        for (auto task : breadth_tasks_) {
            visitor(*task);
        }
    }

    // Visits all tasks in a depth-first traversal.
    template<typename Visitor>
    void visit_depth_all(Visitor& visitor) {
        if (depth_tasks_.empty()) {
            depth_tasks_.reserve(node_->get_id() + 1);
            visit_depth(transwarp::detail::push_task_visitor(depth_tasks_));
            unvisit();
        }
        for (auto task : depth_tasks_) {
            visitor(*task);
        }
    }

    // Visits each task in a depth-first traversal.
    void visit_depth(const std::function<void(transwarp::itask&)>& visitor) override {
        if (!visited_) {
            transwarp::detail::call_with_each(transwarp::detail::visit_depth(visitor), parents_);
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

    // Raises the given event to all listeners
    void raise_event(transwarp::event_type type) {
        for (auto& listener : listeners_) {
            listener->handle_event(type, *this);
        }
    }

    std::shared_ptr<transwarp::node> node_;
    Functor functor_;
    std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...> parents_;
    bool visited_ = false;
    std::shared_ptr<transwarp::executor> executor_;
    std::vector<std::shared_ptr<transwarp::listener>> listeners_;
    std::vector<transwarp::itask*> depth_tasks_;
    std::vector<transwarp::itask*> breadth_tasks_;
};


// A task for non-void result type.
// A task representing a piece of work given by functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Tasks should be created using the make_task factory functions.
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy : public transwarp::detail::task_impl_base<ResultType, TaskType, Functor, ParentResults...> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = ResultType;

    // Assigns a value to this task. Scheduling will have no effect after a value
    // has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(const typename transwarp::remove_refc<result_type>::type& value) override {
        set_value_impl(value);
    }

    // Assigns a value to this task. Scheduling will have no effect after a value
    // has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(typename transwarp::remove_refc<result_type>::type&& value) override {
        set_value_impl(value);
    }

    // Returns the result of this task. Throws any exceptions that the underlying
    // functor throws. Should only be called if was_scheduled() is true,
    // throws transwarp::transwarp_error otherwise
    typename transwarp::result_info<result_type>::type get() const override {
        this->ensure_task_was_scheduled();
        return this->future_.get();
    }

protected:

    template<typename F>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    task_impl_proxy(std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(true, std::move(name), std::forward<F>(functor), std::move(parents)...)
    {}

    template<typename F>
    // cppcheck-suppress uninitMemberVar
    explicit task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(false, "", std::forward<F>(functor), std::move(parents)...)
    {}

private:

    template<typename T>
    void set_value_impl(T&& value) {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_future_with_value<result_type>(std::forward<T>(value));
        this->schedule_mode_ = false;
    }

};

// A task for non-void, non-const reference result type.
// A task representing a piece of work given by functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Tasks should be created using the make_task factory functions.
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy<ResultType&, TaskType, Functor, ParentResults...> : public transwarp::detail::task_impl_base<ResultType&, TaskType, Functor, ParentResults...> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = ResultType&;

    // Assigns a value to this task. Scheduling will have no effect after a value
    // has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(typename transwarp::remove_refc<result_type>::type& value) override {
        set_value_impl(value);
    }

    // Returns the result of this task. Throws any exceptions that the underlying
    // functor throws. Should only be called if was_scheduled() is true,
    // throws transwarp::transwarp_error otherwise
    typename transwarp::result_info<result_type>::type get() const override {
        this->ensure_task_was_scheduled();
        return this->future_.get();
    }

protected:

    template<typename F>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    task_impl_proxy(std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(true, std::move(name), std::forward<F>(functor), std::move(parents)...)
    {}

    template<typename F>
    // cppcheck-suppress uninitMemberVar
    explicit task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(false, "", std::forward<F>(functor), std::move(parents)...)
    {}

private:

    template<typename T>
    void set_value_impl(T&& value) {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_future_with_value<result_type>(std::forward<T>(value));
        this->schedule_mode_ = false;
    }

};

// A task for void result type.
// A task representing a piece of work given by functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Tasks should be created using the make_task factory functions.
template<typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy<void, TaskType, Functor, ParentResults...> : public transwarp::detail::task_impl_base<void, TaskType, Functor, ParentResults...> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = void;

    // Assigns a value to this task. Scheduling will have no effect after a call
    // to this. Calling reset() will reset this and re-enable scheduling.
    void set_value() override {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_ready_future();
        this->schedule_mode_ = false;
    }

    // Blocks until the task finishes. Throws any exceptions that the underlying
    // functor throws. Should only be called if was_scheduled() is true,
    // throws transwarp::transwarp_error otherwise
    void get() const override {
        this->ensure_task_was_scheduled();
        this->future_.get();
    }

protected:

    template<typename F>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    task_impl_proxy(std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(true, std::move(name), std::forward<F>(functor), std::move(parents)...)
    {}

    template<typename F>
    // cppcheck-suppress uninitMemberVar
    explicit task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>(false, "", std::forward<F>(functor), std::move(parents)...)
    {}

};

} // detail


// A task representing a piece of work given by functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Tasks should be created using the make_task factory functions.
template<typename TaskType, typename Functor, typename... ParentResults>
class task_impl : public transwarp::detail::task_impl_proxy<typename transwarp::detail::result<TaskType, Functor, ParentResults...>::type, TaskType, Functor, ParentResults...> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = typename transwarp::detail::result<TaskType, Functor, ParentResults...>::type;

    // A task is defined by name, functor, and parent tasks. name is optional, see overload
    // Note: A task must be created using shared_ptr (because of shared_from_this)
    template<typename F, typename = typename std::enable_if<std::is_same<Functor, typename std::decay<F>::type>::value>::type>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    task_impl(std::string name, F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_proxy<result_type, task_type, Functor, ParentResults...>(std::move(name), std::forward<F>(functor), std::move(parents)...)
    {}

    // This overload is for omitting the task name
    // Note: A task must be created using shared_ptr (because of shared_from_this)
    template<typename F, typename = typename std::enable_if<std::is_same<Functor, typename std::decay<F>::type>::value>::type>
    // cppcheck-suppress uninitMemberVar
    explicit task_impl(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_proxy<result_type, task_type, Functor, ParentResults...>(std::forward<F>(functor), std::move(parents)...)
    {}

    // delete copy/move semantics
    task_impl(const task_impl&) = delete;
    task_impl& operator=(const task_impl&) = delete;
    task_impl(task_impl&&) = delete;
    task_impl& operator=(task_impl&&) = delete;
};


// A value task that stores a single value and doesn't require scheduling.
// Value tasks should be created using the make_value_task factory functions.
template<typename ResultType>
class value_task : public transwarp::task<ResultType> {
public:
    // The task type
    using task_type = transwarp::root_type;

    // The result type of this task
    using result_type = ResultType;

    // A value task is defined by name and value. name is optional, see overload
    template<typename T, typename = typename std::enable_if<std::is_same<result_type, typename std::decay<T>::type>::value>::type>
    // cppcheck-suppress passedByValue
    // cppcheck-suppress uninitMemberVar
    value_task(std::string name, T&& value)
    : value_task(true, std::move(name), std::forward<T>(value))
    {}

    // This overload is for omitting the task name
    template<typename T, typename = typename std::enable_if<std::is_same<result_type, typename std::decay<T>::type>::value>::type>
    // cppcheck-suppress uninitMemberVar
    explicit value_task(T&& value)
    : value_task(false, "", std::forward<T>(value))
    {}

    virtual ~value_task() = default;

    // delete copy/move semantics
    value_task(const value_task&) = delete;
    value_task& operator=(const value_task&) = delete;
    value_task(value_task&&) = delete;
    value_task& operator=(value_task&&) = delete;

    // No-op because a value task never runs
    void set_executor(std::shared_ptr<transwarp::executor>) override {}

    // No-op because a value task never runs and doesn't have parents
    void set_executor_all(std::shared_ptr<transwarp::executor>) override {}

    // No-op because a value task never runs
    void remove_executor() override {}

    // No-op because a value task never runs and doesn't have parents
    void remove_executor_all() override {}

    // Sets a task priority (defaults to 0). transwarp will not directly use this.
    // This is only useful if something else is using the priority
    void set_priority(std::size_t priority) override {
        transwarp::detail::node_manip::set_priority(*node_, priority);
    }

    // Sets a priority to all tasks (defaults to 0). transwarp will not directly use this.
    // This is only useful if something else is using the priority
    void set_priority_all(std::size_t priority) override {
        set_priority(priority);
    }

    // Resets the task priority to 0
    void reset_priority() override {
        transwarp::detail::node_manip::set_priority(*node_, 0);
    }

    // Resets the priority of all tasks to 0
    void reset_priority_all() override {
        reset_priority();
    }

    // Assigns custom data to this task. transwarp will not directly use this.
    // This is only useful if something else is using this custom data
    void set_custom_data(std::shared_ptr<void> custom_data) override {
        if (!custom_data) {
            throw transwarp::transwarp_error("Not a valid pointer to custom data");
        }
        transwarp::detail::node_manip::set_custom_data(*node_, std::move(custom_data));
    }

    // Assigns custom data to all tasks. transwarp will not directly use this.
    // This is only useful if something else is using this custom data
    void set_custom_data_all(std::shared_ptr<void> custom_data) override {
        set_custom_data(std::move(custom_data));
    }

    // Removes custom data from this task
    void remove_custom_data() override {
        transwarp::detail::node_manip::set_custom_data(*node_, nullptr);
    }

    // Removes custom data from all tasks
    void remove_custom_data_all() override {
        remove_custom_data();
    }

    // Returns the future associated to the underlying execution
    const std::shared_future<result_type>& get_future() const noexcept override {
        return future_;
    }

    // Returns the associated node
    const std::shared_ptr<transwarp::node>& get_node() const noexcept override {
        return node_;
    }

    // No-op because a value task never runs
    void schedule() override {}

    // No-op because a value task never runs
    void schedule(transwarp::executor&) override {}

    // No-op because a value task never runs
    void schedule(bool) override {}

    // No-op because a value task never runs
    void schedule(transwarp::executor&, bool) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all() override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(bool) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&, bool) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::schedule_type) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&, transwarp::schedule_type) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::schedule_type, bool) override {}

    // No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&, transwarp::schedule_type, bool) override {}

    // Assigns a value to this task
    void set_value(const typename transwarp::remove_refc<result_type>::type& value) override {
        future_ = transwarp::detail::make_future_with_value<result_type>(value);
    }

    // Assigns a value to this task
    void set_value(typename transwarp::remove_refc<result_type>::type&& value) override {
        future_ = transwarp::detail::make_future_with_value<result_type>(value);
    };

    // Assigns an exception to this task
    void set_exception(std::exception_ptr exception) override {
        future_ = transwarp::detail::make_future_with_exception<result_type>(exception);
    }

    // Returns true because a value task is scheduled once on construction
    bool was_scheduled() const noexcept override {
        return true;
    }

    // No-op because a value task never runs
    void wait() const override {}

    // Returns true because a value task is always ready
    bool is_ready() const override {
        return true;
    }

    // Returns true because a value task always contains a result
    bool has_result() const noexcept override {
        return true;
    }

    // No-op because a value task doesn't raise events
    void add_listener(std::shared_ptr<transwarp::listener>) override {}

    // No-op because a value task doesn't raise events
    void remove_listener(const std::shared_ptr<transwarp::listener>&) override {}

    // Returns the result of this task
    typename transwarp::result_info<result_type>::type get() const override {
        return future_.get();
    }

    // No-op because a value task never runs
    void reset() override {}

    // No-op because a value task never runs and doesn't have parents
    void reset_all() override {}

    // No-op because a value task never runs
    void cancel(bool) noexcept override {}

    // No-op because a value task never runs and doesn't have parents
    void cancel_all(bool) noexcept override {}

    // Returns an empty graph because a value task doesn't have parents
    std::vector<transwarp::edge> get_graph() const override {
        return {};
    }

private:

    template<typename T>
    // cppcheck-suppress passedByValue
    value_task(bool has_name, std::string name, T&& value)
    : node_(std::make_shared<transwarp::node>()),
      future_(transwarp::detail::make_future_with_value<result_type>(std::forward<T>(value)))
    {
        transwarp::detail::node_manip::set_type(*node_, task_type::value);
        transwarp::detail::node_manip::set_name(*node_, (has_name ? std::make_shared<std::string>(std::move(name)) : nullptr));
    }

    // Assigns the given id to the node
    void set_node_id(std::size_t id) noexcept override {
        transwarp::detail::node_manip::set_id(*node_, id);
    }

    // No-op because a value task never runs
    void schedule_impl(bool, transwarp::executor*) override {}

    // Visits this task
    void visit_depth(const std::function<void(transwarp::itask&)>& visitor) override {
        if (!visited_) {
            visitor(*this);
            visited_ = true;
        }
    }

    // Marks this task as not visited
    void unvisit() noexcept override {
        visited_ = false;
    }

    std::shared_ptr<transwarp::node> node_;
    std::shared_future<result_type> future_;
    bool visited_ = false;
};


// A factory function to create a new task
template<typename TaskType, typename Functor, typename... Parents>
auto make_task(TaskType, std::string name, Functor&& functor, std::shared_ptr<Parents>... parents)
    -> decltype(std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::move(name), std::forward<Functor>(functor), std::move(parents)...)) {
    return std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::move(name), std::forward<Functor>(functor), std::move(parents)...);
}

// A factory function to create a new task. Overload for omitting the task name
template<typename TaskType, typename Functor, typename... Parents>
auto make_task(TaskType, Functor&& functor, std::shared_ptr<Parents>... parents)
    -> decltype(std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::forward<Functor>(functor), std::move(parents)...)) {
    return std::make_shared<transwarp::task_impl<TaskType, typename std::decay<Functor>::type, typename Parents::result_type...>>(std::forward<Functor>(functor), std::move(parents)...);
}


// A factory function to create a new value task
template<typename Value>
auto make_value_task(std::string name, Value&& value)
    -> decltype(std::make_shared<transwarp::value_task<typename std::decay<Value>::type>>(std::move(name), std::forward<Value>(value))) {
    return std::make_shared<transwarp::value_task<typename std::decay<Value>::type>>(std::move(name), std::forward<Value>(value));
}

// A factory function to create a new value task. Overload for omitting the task name
template<typename Value>
auto make_value_task(Value&& value)
    -> decltype(std::make_shared<transwarp::value_task<typename std::decay<Value>::type>>(std::forward<Value>(value))) {
    return std::make_shared<transwarp::value_task<typename std::decay<Value>::type>>(std::forward<Value>(value));
}


} // transwarp
