/// @mainpage transwarp is a header-only C++ library for task concurrency
/// @details https://github.com/bloomen/transwarp
/// @version 2.0.0
/// @author Christian Blume, Guan Wang
/// @date 2019
/// @copyright MIT http://www.opensource.org/licenses/mit-license.php
#pragma once
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>


/// The transwarp namespace
namespace transwarp {


/// The possible task types
enum class task_type {
    root,        ///< The task has no parents
    accept,      ///< The task's functor accepts all parent futures
    accept_any,  ///< The task's functor accepts the first parent future that becomes ready
    consume,     ///< The task's functor consumes all parent results
    consume_any, ///< The task's functor consumes the first parent result that becomes ready
    wait,        ///< The task's functor takes no arguments but waits for all parents to finish
    wait_any,    ///< The task's functor takes no arguments but waits for the first parent to finish
};


/// Base class for exceptions
class transwarp_error : public std::runtime_error {
public:
    explicit transwarp_error(const std::string& message)
    : std::runtime_error{message}
    {}
};


/// Exception thrown when a task is canceled
class task_canceled : public transwarp::transwarp_error {
public:
    explicit task_canceled(const std::string& node_repr)
    : transwarp::transwarp_error{"Task canceled: " + node_repr}
    {}
};


/// Exception thrown when a task was destroyed prematurely
class task_destroyed : public transwarp::transwarp_error {
public:
    explicit task_destroyed(const std::string& node_repr)
    : transwarp::transwarp_error{"Task destroyed: " + node_repr}
    {}
};


/// Exception thrown when an invalid parameter was passed to a function
class invalid_parameter : public transwarp::transwarp_error {
public:
    explicit invalid_parameter(const std::string& parameter)
    : transwarp::transwarp_error{"Invalid parameter: " + parameter}
    {}
};


/// Exception thrown when a task is used in unintended ways
class control_error : public transwarp::transwarp_error {
public:
    explicit control_error(const std::string& message)
    : transwarp::transwarp_error{"Control error: " + message}
    {}
};


/// String conversion for the task_type enumeration
inline std::string to_string(const transwarp::task_type& type) {
    switch (type) {
    case transwarp::task_type::root: return "root";
    case transwarp::task_type::accept: return "accept";
    case transwarp::task_type::accept_any: return "accept_any";
    case transwarp::task_type::consume: return "consume";
    case transwarp::task_type::consume_any: return "consume_any";
    case transwarp::task_type::wait: return "wait";
    case transwarp::task_type::wait_any: return "wait_any";
    default: throw transwarp::invalid_parameter{"task type"};
    }
}


/// The root type. Used for tag dispatch
struct root_type : std::integral_constant<transwarp::task_type, transwarp::task_type::root> {};
constexpr transwarp::root_type root{}; ///< The root task tag

/// The accept type. Used for tag dispatch
struct accept_type : std::integral_constant<transwarp::task_type, transwarp::task_type::accept> {};
constexpr transwarp::accept_type accept{}; ///< The accept task tag

/// The accept_any type. Used for tag dispatch
struct accept_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::accept_any> {};
constexpr transwarp::accept_any_type accept_any{}; ///< The accept_any task tag

/// The consume type. Used for tag dispatch
struct consume_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume> {};
constexpr transwarp::consume_type consume{}; ///< The consume task tag

/// The consume_any type. Used for tag dispatch
struct consume_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume_any> {};
constexpr transwarp::consume_any_type consume_any{}; ///< The consume_any task tag

/// The wait type. Used for tag dispatch
struct wait_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait> {};
constexpr transwarp::wait_type wait{}; ///< The wait task tag

/// The wait_any type. Used for tag dispatch
struct wait_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait_any> {};
constexpr transwarp::wait_any_type wait_any{}; ///< The wait_any task tag


/// Detail namespace for internal functionality only
namespace detail {

struct visit_visitor;
struct unvisit_visitor;
struct final_visitor;
struct schedule_visitor;
struct node_manip;

} // detail


/// A node carrying meta-data of a task
class node {
public:

    node() = default;

    // delete copy/move semantics
    node(const node&) = delete;
    node& operator=(const node&) = delete;
    node(node&&) = delete;
    node& operator=(node&&) = delete;

    /// Clones this node
    std::unique_ptr<node> clone() const {
        // copies everything but parents
        auto n = std::unique_ptr<node>{new node};
        n->id_ = id_;
        n->level_ = level_;
        n->type_ = type_;
        n->name_ = name_;
        n->executor_ = executor_;
        n->priority_ = priority_;
        n->custom_data_ = custom_data_;
        n->canceled_ = canceled_.load();
        n->avg_idletime_us_ = avg_idletime_us_.load();
        n->avg_waittime_us_ = avg_waittime_us_.load();
        n->avg_runtime_us_ = avg_runtime_us_.load();
        return n;
    }

    /// The task ID
    std::size_t id() const noexcept {
        return id_;
    }

    /// The task level
    std::size_t level() const noexcept {
        return level_;
    }

    /// The task type
    transwarp::task_type type() const noexcept {
        return type_;
    }

    /// The optional task name
    const std::optional<std::string>& name() const noexcept {
        return name_;
    }

    /// The optional, task-specific executor
    const std::optional<std::string>& executor() const noexcept {
        return executor_;
    }

    /// The task's parents (may be empty)
    const std::vector<std::shared_ptr<node>>& parents() const noexcept {
        return parents_;
    }

    /// The task priority (defaults to 0)
    std::int64_t priority() const noexcept {
        return priority_;
    }

    /// The custom task data (may not hold a value)
    const std::any& custom_data() const noexcept {
        return custom_data_;
    }

    /// Returns whether the associated task is canceled
    bool canceled() const noexcept {
        return canceled_.load();
    }

    /// Returns the average idletime in microseconds (-1 if never set)
    std::int64_t avg_idletime_us() const noexcept {
        return avg_idletime_us_.load();
    }

    /// Returns the average waittime in microseconds (-1 if never set)
    std::int64_t avg_waittime_us() const noexcept {
        return avg_waittime_us_.load();
    }

    /// Returns the average runtime in microseconds (-1 if never set)
    std::int64_t avg_runtime_us() const noexcept {
        return avg_runtime_us_.load();
    }

private:
    friend struct transwarp::detail::node_manip;

    std::size_t id_ = 0;
    std::size_t level_ = 0;
    transwarp::task_type type_ = transwarp::task_type::root;
    std::optional<std::string> name_;
    std::optional<std::string> executor_;
    std::vector<std::shared_ptr<node>> parents_;
    std::int64_t priority_ = 0;
    std::any custom_data_;
    std::atomic<bool> canceled_{false};
    std::atomic<std::int64_t> avg_idletime_us_{-1};
    std::atomic<std::int64_t> avg_waittime_us_{-1};
    std::atomic<std::int64_t> avg_runtime_us_{-1};
};

/// String conversion for the node class
inline std::string to_string(const transwarp::node& node, std::string_view separator="\n") {
    std::string s;
    s += '"';
    const std::optional<std::string>& name = node.name();
    if (name) {
        s += std::string{"<"} + *name + std::string{">"} + separator.data();
    }
    s += transwarp::to_string(node.type());
    s += std::string{" id="} + std::to_string(node.id());
    s += std::string{" lev="} + std::to_string(node.level());
    const std::optional<std::string>& exec = node.executor();
    if (exec) {
        s += separator.data() + std::string{"<"} + *exec + std::string{">"};
    }
    const std::int64_t avg_idletime_us = node.avg_idletime_us();
    if (avg_idletime_us >= 0) {
        s += separator.data() + std::string{"avg-idle-us="} + std::to_string(avg_idletime_us);
    }
    const std::int64_t avg_waittime_us = node.avg_waittime_us();
    if (avg_waittime_us >= 0) {
        s += separator.data() + std::string{"avg-wait-us="} + std::to_string(avg_waittime_us);
    }
    const std::int64_t avg_runtime_us = node.avg_runtime_us();
    if (avg_runtime_us >= 0) {
        s += separator.data() + std::string{"avg-run-us="} + std::to_string(avg_runtime_us);
    }
    return s + '"';
}


/// An edge between two nodes
class edge {
public:
    edge(std::shared_ptr<transwarp::node> parent, std::shared_ptr<transwarp::node> child) noexcept
    : parent_{std::move(parent)}, child_{std::move(child)}
    {}

    // default copy/move semantics
    edge(const edge&) = default;
    edge& operator=(const edge&) = default;
    edge(edge&&) = default;
    edge& operator=(edge&&) = default;

    /// Returns the parent node
    const std::shared_ptr<transwarp::node>& parent() const noexcept {
        return parent_;
    }

    /// Returns the child node
    const std::shared_ptr<transwarp::node>& child() const noexcept {
        return child_;
    }

private:
    std::shared_ptr<transwarp::node> parent_;
    std::shared_ptr<transwarp::node> child_;
};

/// String conversion for the edge class
inline std::string to_string(const transwarp::edge& edge, std::string_view separator="\n") {
    return transwarp::to_string(*edge.parent(), separator) + std::string{" -> "} + transwarp::to_string(*edge.child(), separator);
}


/// Creates a dot-style string from the given edges
inline std::string to_string(const std::vector<transwarp::edge>& edges, std::string_view separator="\n") {
    std::string dot = std::string{"digraph {"} + separator.data();
    for (const transwarp::edge& edge : edges) {
        dot += transwarp::to_string(edge, separator) + separator.data();
    }
    dot += std::string{"}"};
    return dot;
}


/// The task events that can be subscribed to using the listener interface
enum class event_type {
    before_scheduled, ///< Just before a task is scheduled (handle_event called on thread of caller to schedule())
    before_started,   ///< Just before a task starts running (handle_event called on thread that task is run on)
    before_invoked,   ///< Just before a task's functor is invoked (handle_event called on thread that task is run on)
    after_finished,   ///< Just after a task has finished running (handle_event called on thread that task is run on)
    after_canceled,   ///< Just after a task was canceled (handle_event called on thread that task is run on)
    count,
};


class listener;
class executor;

/// An interface for the task class
class itask {
public:
    virtual ~itask() = default;

    virtual void finalize() = 0;
    virtual void set_executor(std::shared_ptr<transwarp::executor> executor) = 0;
    virtual void set_executor_all(std::shared_ptr<transwarp::executor> executor) = 0;
    virtual void remove_executor() = 0;
    virtual void remove_executor_all() = 0;
    virtual void set_priority(std::int64_t priority) = 0;
    virtual void set_priority_all(std::int64_t priority) = 0;
    virtual void reset_priority() = 0;
    virtual void reset_priority_all() = 0;
    virtual void set_custom_data(std::any custom_data) = 0;
    virtual void set_custom_data_all(std::any custom_data) = 0;
    virtual void remove_custom_data() = 0;
    virtual void remove_custom_data_all() = 0;
    virtual const std::shared_ptr<transwarp::node>& node() const noexcept = 0;
    virtual void add_listener(std::shared_ptr<transwarp::listener> listener) = 0;
    virtual void add_listener(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener) = 0;
    virtual void add_listener_all(std::shared_ptr<transwarp::listener> listener) = 0;
    virtual void add_listener_all(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener) = 0;
    virtual void remove_listener(const std::shared_ptr<transwarp::listener>& listener) = 0;
    virtual void remove_listener(transwarp::event_type event, const std::shared_ptr<transwarp::listener>& listener) = 0;
    virtual void remove_listener_all(const std::shared_ptr<transwarp::listener>& listener) = 0;
    virtual void remove_listener_all(transwarp::event_type event, const std::shared_ptr<transwarp::listener>& listener) = 0;
    virtual void remove_listeners() = 0;
    virtual void remove_listeners(transwarp::event_type event) = 0;
    virtual void remove_listeners_all() = 0;
    virtual void remove_listeners_all(transwarp::event_type event) = 0;
    virtual void schedule() = 0;
    virtual void schedule(transwarp::executor& executor) = 0;
    virtual void schedule(bool reset) = 0;
    virtual void schedule(transwarp::executor& executor, bool reset) = 0;
    virtual void schedule_all() = 0;
    virtual void schedule_all(transwarp::executor& executor) = 0;
    virtual void schedule_all(bool reset_all) = 0;
    virtual void schedule_all(transwarp::executor& executor, bool reset_all) = 0;
    virtual void set_exception(std::exception_ptr exception) = 0;
    virtual bool was_scheduled() const noexcept = 0;
    virtual void wait() const = 0;
    virtual bool is_ready() const = 0;
    virtual bool has_result() const = 0;
    virtual void reset() = 0;
    virtual void reset_all() = 0;
    virtual void cancel(bool enabled) noexcept = 0;
    virtual void cancel_all(bool enabled) noexcept = 0;
    virtual const std::vector<itask*>& tasks() = 0;
    virtual std::vector<transwarp::edge> edges() = 0;

protected:
    virtual void schedule_impl(bool reset, transwarp::executor* executor=nullptr) = 0;

private:
    friend struct transwarp::detail::visit_visitor;
    friend struct transwarp::detail::unvisit_visitor;
    friend struct transwarp::detail::final_visitor;
    friend struct transwarp::detail::schedule_visitor;

    virtual void visit(const std::function<void(itask&)>& visitor) = 0;
    virtual void unvisit() noexcept = 0;
    virtual void set_node_id(std::size_t id) noexcept = 0;
};


/// The executor interface used to perform custom task execution
class executor {
public:
    virtual ~executor() = default;

    /// Returns the name of the executor
    virtual std::string name() const = 0;

    /// Runs a task which is wrapped by the given functor. The functor only
    /// captures one shared pointer and can hence be copied at low cost.
    /// task represents the task that the functor belongs to.
    /// This function is only ever called on the thread of the caller to schedule().
    /// The implementer needs to ensure that this never throws exceptions
    virtual void execute(const std::function<void()>& functor, const transwarp::itask& task) = 0;
};


/// The listener interface to listen to events raised by tasks
class listener {
public:
    virtual ~listener() = default;

    /// This may be called from arbitrary threads depending on the event type (see transwarp::event_type).
    /// The implementer needs to ensure that this never throws exceptions. The lifetime of the task
    /// reference is not guaranteed beyond the duration of handle_event, and listeners must not retain
    /// a copy of the task.
    virtual void handle_event(transwarp::event_type event, const transwarp::itask& task) = 0;
};


/// Removes reference and const from a type
template<typename T>
using decay_t = std::remove_const_t<std::remove_reference_t<T>>;


/// Returns the result type of a std::shared_future<T>
template<typename T>
using result_t = std::result_of_t<decltype(&std::shared_future<T>::get)(std::shared_future<T>)>;


/// Clones a task
template<typename TaskType>
std::shared_ptr<TaskType> clone_task(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const std::shared_ptr<TaskType>& t) {
    const auto original_task = std::dynamic_pointer_cast<transwarp::itask>(t);
    const auto task_cache_it = task_cache.find(original_task);
    if (task_cache_it != task_cache.cend()) {
        return std::dynamic_pointer_cast<TaskType>(task_cache_it->second);
    } else {
        auto cloned_task = t->clone_impl(task_cache);
        task_cache[original_task] = cloned_task;
        return std::move(cloned_task);
    }
}


/// The task class
template<typename ResultType>
class task : public transwarp::itask {
public:
    using result_type = ResultType;

    virtual ~task() = default;

    std::shared_ptr<task> clone() const {
        std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>> task_cache;
        return clone_impl(task_cache);
    }

    virtual void set_value(const transwarp::decay_t<result_type>& value) = 0;
    virtual void set_value(transwarp::decay_t<result_type>&& value) = 0;
    virtual const std::shared_future<result_type>& future() const noexcept = 0;
    virtual transwarp::result_t<result_type> get() const = 0;

private:
    template<typename T>
    friend std::shared_ptr<T> clone_task(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const std::shared_ptr<T>& t);

    virtual std::shared_ptr<transwarp::task<result_type>> clone_impl(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache) const = 0;
};

/// The task class (reference result type)
template<typename ResultType>
class task<ResultType&> : public transwarp::itask {
public:
    using result_type = ResultType&;

    virtual ~task() = default;

    std::shared_ptr<task> clone() const {
        std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>> task_cache;
        return clone_impl(task_cache);
    }

    virtual void set_value(transwarp::decay_t<result_type>& value) = 0;
    virtual const std::shared_future<result_type>& future() const noexcept = 0;
    virtual transwarp::result_t<result_type> get() const = 0;

private:
    template<typename T>
    friend std::shared_ptr<T> clone_task(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const std::shared_ptr<T>& t);

    virtual std::shared_ptr<transwarp::task<result_type>> clone_impl(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache) const = 0;
};

/// The task class (void result type)
template<>
class task<void> : public transwarp::itask {
public:
    using result_type = void;

    virtual ~task() = default;

    std::shared_ptr<task> clone() const {
        std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>> task_cache;
        return clone_impl(task_cache);
    }

    virtual void set_value() = 0;
    virtual const std::shared_future<result_type>& future() const noexcept = 0;
    virtual result_type get() const = 0;

private:
    template<typename T>
    friend std::shared_ptr<T> clone_task(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const std::shared_ptr<T>& t);

    virtual std::shared_ptr<transwarp::task<result_type>> clone_impl(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache) const = 0;
};


/// Detail namespace for internal functionality only
namespace detail {

template<typename F>
void assign_node_if(F&, const std::shared_ptr<transwarp::node>&) noexcept;

} // detail


/// A base class for a user-defined functor that needs access to the node associated
/// to the task or a cancel point to stop a task while it's running
class functor {
public:

    virtual ~functor() = default;

protected:

    /// The node associated to the task
    const std::shared_ptr<transwarp::node>& transwarp_node() const noexcept {
        return transwarp_node_;
    }

    /// If the associated task is canceled then this will throw transwarp::task_canceled
    /// which will stop the task while it's running
    void transwarp_cancel_point() const {
        if (transwarp_node_->canceled()) {
            throw transwarp::task_canceled(std::to_string(transwarp_node_->id()));
        }
    }

private:
    template<typename F>
    friend void transwarp::detail::assign_node_if(F&, const std::shared_ptr<transwarp::node>&) noexcept;

    std::shared_ptr<transwarp::node> transwarp_node_;
};


/// Detail namespace for internal functionality only
namespace detail {


/// Node manipulation
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

    static void set_name(transwarp::node& node, std::optional<std::string> name) noexcept {
        node.name_ = std::move(name);
    }

    static void set_executor(transwarp::node& node, std::optional<std::string> executor) noexcept {
        node.executor_ = std::move(executor);
    }

    static void add_parent(transwarp::node& node, std::shared_ptr<transwarp::node> parent) {
        node.parents_.push_back(std::move(parent));
    }

    static void set_priority(transwarp::node& node, std::int64_t priority) noexcept {
        node.priority_ = priority;
    }

    static void set_custom_data(transwarp::node& node, std::any custom_data) {
        node.custom_data_ = std::move(custom_data);
    }

    static void set_canceled(transwarp::node& node, bool enabled) noexcept {
        node.canceled_ = enabled;
    }

    static void set_avg_idletime_us(transwarp::node& node, std::int64_t idletime) noexcept {
        node.avg_idletime_us_ = idletime;
    }

    static void set_avg_waittime_us(transwarp::node& node, std::int64_t waittime) noexcept {
        node.avg_waittime_us_ = waittime;
    }

    static void set_avg_runtime_us(transwarp::node& node, std::int64_t runtime) noexcept {
        node.avg_runtime_us_ = runtime;
    }

};


/// A simple thread pool used to execute tasks in parallel
class thread_pool {
public:

    explicit thread_pool(std::size_t n_threads)
    {
        if (n_threads == 0) {
            throw transwarp::invalid_parameter{"number of threads"};
        }
        const std::size_t n_target = threads_.size() + n_threads;
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
            std::lock_guard<std::mutex> lock{mutex_};
            functors_.push(functor);
        }
        cond_var_.notify_one();
    }

private:

    void worker() {
        for (;;) {
            std::function<void()> functor;
            {
                std::unique_lock<std::mutex> lock{mutex_};
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
            std::lock_guard<std::mutex> lock{mutex_};
            done_ = true;
        }
        cond_var_.notify_all();
        for (std::thread& thread : threads_) {
            thread.join();
        }
        threads_.clear();
    }

    bool done_ = false;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> functors_;
    std::condition_variable cond_var_;
    std::mutex mutex_;
};


/// Applies the functor to each element in the tuple
template<typename Functor, typename Tuple>
void apply_to_each(Functor&& f, Tuple&& t) {
    std::apply([&f](auto&&... arg){(..., f(std::forward<decltype(arg)>(arg)));}, std::forward<Tuple>(t));
}


template<int offset, typename... ParentResults>
struct assign_futures_impl {
    static void work(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& source, std::tuple<std::shared_future<ParentResults>...>& target) {
        std::get<offset>(target) = std::get<offset>(source)->future();
        assign_futures_impl<offset - 1, ParentResults...>::work(source, target);
    }
};

template<typename... ParentResults>
struct assign_futures_impl<-1, ParentResults...> {
    static void work(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>&, std::tuple<std::shared_future<ParentResults>...>&) {}
};

/// Returns the futures from the given tuple of tasks
template<typename... ParentResults>
std::tuple<std::shared_future<ParentResults>...> get_futures(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& input) {
    std::tuple<std::shared_future<ParentResults>...> result;
    assign_futures_impl<static_cast<int>(sizeof...(ParentResults)) - 1, ParentResults...>::work(input, result);
    return result;
}

/// Returns the futures from the given vector of tasks
template<typename ParentResultType>
std::vector<std::shared_future<ParentResultType>> get_futures(const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& input) {
    std::vector<std::shared_future<ParentResultType>> result;
    result.reserve(input.size());
    for (const std::shared_ptr<transwarp::task<ParentResultType>>& task : input) {
        result.emplace_back(task->future());
    }
    return result;
}


/// Runs the task with the given arguments, hence, invoking the task's functor
template<typename Result, typename Task, typename... Args>
Result run_task(std::size_t node_id, const std::weak_ptr<Task>& task, Args&&... args) {
    const std::shared_ptr<Task> t = task.lock();
    if (!t) {
        throw transwarp::task_destroyed{std::to_string(node_id)};
    }
    if (t->node_->canceled()) {
        throw transwarp::task_canceled{std::to_string(node_id)};
    }
    t->raise_event(transwarp::event_type::before_invoked);
    return (*t->functor_)(std::forward<Args>(args)...);
}


/// Waits for all parents to finish
template<typename... ParentResults>
void wait_for_all(const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
    transwarp::detail::apply_to_each([](auto& p){ p->future().wait(); }, parents);
}


/// Waits for all parents to finish
template<typename ParentResultType>
void wait_for_all(const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
    for (const std::shared_ptr<transwarp::task<ParentResultType>>& parent : parents) {
        parent->future().wait();
    }
}


template<typename Parent>
Parent wait_for_any_impl() {
    return {};
}

template<typename Parent, typename ParentResult, typename... ParentResults>
Parent wait_for_any_impl(const std::shared_ptr<transwarp::task<ParentResult>>& parent, const std::shared_ptr<transwarp::task<ParentResults>>& ...parents) {
    const std::future_status status = parent->future().wait_for(std::chrono::microseconds(1));
    if (status == std::future_status::ready) {
        return parent;
    }
    return transwarp::detail::wait_for_any_impl<Parent>(parents...);
}

/// Waits for the first parent to finish
template<typename Parent, typename... ParentResults>
Parent wait_for_any(const std::shared_ptr<transwarp::task<ParentResults>>& ...parents) {
    for (;;) {
        Parent parent = transwarp::detail::wait_for_any_impl<Parent>(parents...);
        if (parent) {
            return parent;
        }
    }
}


/// Waits for the first parent to finish
template<typename ParentResultType>
std::shared_ptr<transwarp::task<ParentResultType>> wait_for_any(const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
    for (;;) {
        for (const std::shared_ptr<transwarp::task<ParentResultType>>& parent : parents) {
            const std::future_status status = parent->future().wait_for(std::chrono::microseconds(1));
            if (status == std::future_status::ready) {
                return parent;
            }
        }
    }
}


/// Cancels all tasks but one
template<typename OneResult, typename... ParentResults>
void cancel_all_but_one(const std::shared_ptr<transwarp::task<OneResult>>& one, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
    auto callable = [&one](auto& parent) {
        if (one != parent) {
            parent->cancel(true);
        }
    };
    transwarp::detail::apply_to_each(callable, parents);
}


/// Cancels all tasks but one
template<typename OneResult, typename ParentResultType>
void cancel_all_but_one(const std::shared_ptr<transwarp::task<OneResult>>& one, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
    for (const std::shared_ptr<transwarp::task<ParentResultType>>& parent : parents) {
        if (one != parent) {
            parent->cancel(true);
        }
    }
}


template<typename TaskType, bool done, int total, int... n>
struct call_impl {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        return call_impl<TaskType, total == 1 + static_cast<int>(sizeof...(n)), total, n..., static_cast<int>(sizeof...(n))>::template
                work<Result>(node_id, task, parents);
    }
};

template<typename TaskType>
struct call_impl_vector;

template<int total, int... n>
struct call_impl<transwarp::root_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>&) {
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<>
struct call_impl_vector<transwarp::root_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>&) {
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<int total, int... n>
struct call_impl<transwarp::accept_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        transwarp::detail::wait_for_all(parents);
        const std::tuple<std::shared_future<ParentResults>...> futures = transwarp::detail::get_futures(parents);
        return transwarp::detail::run_task<Result>(node_id, task, std::get<n>(futures)...);
    }
};

template<>
struct call_impl_vector<transwarp::accept_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        transwarp::detail::wait_for_all(parents);
        return transwarp::detail::run_task<Result>(node_id, task, transwarp::detail::get_futures(parents));
    }
};

template<int total, int... n>
struct call_impl<transwarp::accept_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        using parent_t = std::remove_reference_t<decltype(std::get<0>(parents))>; // Use first type as reference
        parent_t parent = transwarp::detail::wait_for_any<parent_t>(std::get<n>(parents)...);
        transwarp::detail::cancel_all_but_one(parent, parents);
        return transwarp::detail::run_task<Result>(node_id, task, parent->future());
    }
};

template<>
struct call_impl_vector<transwarp::accept_any_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        std::shared_ptr<transwarp::task<ParentResultType>> parent = transwarp::detail::wait_for_any(parents);
        transwarp::detail::cancel_all_but_one(parent, parents);
        return transwarp::detail::run_task<Result>(node_id, task, parent->future());
    }
};

template<int total, int... n>
struct call_impl<transwarp::consume_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        transwarp::detail::wait_for_all(parents);
        return transwarp::detail::run_task<Result>(node_id, task, std::get<n>(parents)->future().get()...);
    }
};

template<>
struct call_impl_vector<transwarp::consume_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        transwarp::detail::wait_for_all(parents);
        std::vector<ParentResultType> results;
        results.reserve(parents.size());
        for (const std::shared_ptr<transwarp::task<ParentResultType>>& parent : parents) {
            results.emplace_back(parent->future().get());
        }
        return transwarp::detail::run_task<Result>(node_id, task, std::move(results));
    }
};

template<int total, int... n>
struct call_impl<transwarp::consume_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        using parent_t = std::remove_reference_t<decltype(std::get<0>(parents))>; /// Use first type as reference
        parent_t parent = transwarp::detail::wait_for_any<parent_t>(std::get<n>(parents)...);
        transwarp::detail::cancel_all_but_one(parent, parents);
        return transwarp::detail::run_task<Result>(node_id, task, parent->future().get());
    }
};

template<>
struct call_impl_vector<transwarp::consume_any_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        std::shared_ptr<transwarp::task<ParentResultType>> parent = transwarp::detail::wait_for_any(parents);
        transwarp::detail::cancel_all_but_one(parent, parents);
        return transwarp::detail::run_task<Result>(node_id, task, parent->future().get());
    }
};

template<int total, int... n>
struct call_impl<transwarp::wait_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        transwarp::detail::wait_for_all(parents);
        transwarp::detail::apply_to_each([](auto& p){ p->future().get(); }, parents); // Ensures that exceptions are propagated
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<>
struct call_impl_vector<transwarp::wait_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        transwarp::detail::wait_for_all(parents);
        for (const std::shared_ptr<transwarp::task<ParentResultType>>& parent : parents) {
            parent->future().get(); // Ensures that exceptions are propagated
        }
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<int total, int... n>
struct call_impl<transwarp::wait_any_type, true, total, n...> {
    template<typename Result, typename Task, typename... ParentResults>
    static Result work(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
        using parent_t = std::remove_reference_t<decltype(std::get<0>(parents))>; // Use first type as reference
        parent_t parent = transwarp::detail::wait_for_any<parent_t>(std::get<n>(parents)...);
        transwarp::detail::cancel_all_but_one(parent, parents);
        parent->future().get(); // Ensures that exceptions are propagated
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

template<>
struct call_impl_vector<transwarp::wait_any_type> {
    template<typename Result, typename Task, typename ParentResultType>
    static Result work(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
        std::shared_ptr<transwarp::task<ParentResultType>> parent = transwarp::detail::wait_for_any(parents);
        transwarp::detail::cancel_all_but_one(parent, parents);
        parent->future().get(); // Ensures that exceptions are propagated
        return transwarp::detail::run_task<Result>(node_id, task);
    }
};

/// Calls the functor of the given task with the results from the tuple of parents.
/// Throws transwarp::task_canceled if the task is canceled.
/// Throws transwarp::task_destroyed in case the task was destroyed prematurely.
template<typename TaskType, typename Result, typename Task, typename... ParentResults>
Result call(std::size_t node_id, const Task& task, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& parents) {
    constexpr std::size_t n = std::tuple_size_v<std::tuple<std::shared_future<ParentResults>...>>;
    return transwarp::detail::call_impl<TaskType, 0 == n, static_cast<int>(n)>::template
            work<Result>(node_id, task, parents);
}

/// Calls the functor of the given task with the results from the vector of parents.
/// Throws transwarp::task_canceled if the task is canceled.
/// Throws transwarp::task_destroyed in case the task was destroyed prematurely.
template<typename TaskType, typename Result, typename Task, typename ParentResultType>
Result call(std::size_t node_id, const Task& task, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& parents) {
    return transwarp::detail::call_impl_vector<TaskType>::template
            work<Result>(node_id, task, parents);
}


/// Calls the functor with every element in the tuple
template<typename Functor, typename... ParentResults>
void call_with_each(const Functor& f, const std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>& t) {
    auto callable = [&f](const auto& task) {
        if (!task) {
            throw transwarp::invalid_parameter{"task pointer"};
        }
        f(*task);
    };
    transwarp::detail::apply_to_each(callable, t);
}

/// Calls the functor with every element in the vector
template<typename Functor, typename ParentResultType>
void call_with_each(const Functor& f, const std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>& v) {
    for (const std::shared_ptr<transwarp::task<ParentResultType>>& ptr : v) {
        if (!ptr) {
            throw transwarp::invalid_parameter{"task pointer"};
        }
        f(*ptr);
    }
}


/// Sets parents and level of the node
struct parent_visitor {
    explicit parent_visitor(transwarp::node& node) noexcept
    : node_(node) {}

    void operator()(const transwarp::itask& task) const {
        transwarp::detail::node_manip::add_parent(node_, task.node());
        if (node_.level() <= task.node()->level()) {
            /// A child's level is always larger than any of its parents' levels
            transwarp::detail::node_manip::set_level(node_, task.node()->level() + 1);
        }
    }

    transwarp::node& node_;
};

/// Applies final bookkeeping to the task and collects the task
struct final_visitor {
    explicit final_visitor(std::vector<transwarp::itask*>& tasks) noexcept
    : tasks_{tasks} {}

    void operator()(transwarp::itask& task) noexcept {
        tasks_.push_back(&task);
        task.set_node_id(id_++);
    }

    std::vector<transwarp::itask*>& tasks_;
    std::size_t id_ = 0;
};

/// Generates edges
struct edges_visitor {
    explicit edges_visitor(std::vector<transwarp::edge>& edges) noexcept
    : edges_{edges} {}

    void operator()(const transwarp::itask& task) {
        const std::shared_ptr<transwarp::node>& node = task.node();
        for (const std::shared_ptr<transwarp::node>& parent : node->parents()) {
            edges_.emplace_back(parent, node);
        }
    }

    std::vector<transwarp::edge>& edges_;
};

/// Schedules using the given executor
struct schedule_visitor {
    schedule_visitor(bool reset, transwarp::executor* executor) noexcept
    : reset_{reset}, executor_{executor} {}

    void operator()(transwarp::itask& task) {
        task.schedule_impl(reset_, executor_);
    }

    bool reset_;
    transwarp::executor* executor_;
};

/// Resets the given task
struct reset_visitor {

    void operator()(transwarp::itask& task) const {
        task.reset();
    }
};

/// Cancels or resumes the given task
struct cancel_visitor {
    explicit cancel_visitor(bool enabled) noexcept
    : enabled_{enabled} {}

    void operator()(transwarp::itask& task) const noexcept {
        task.cancel(enabled_);
    }

    bool enabled_;
};

/// Assigns an executor to the given task
struct set_executor_visitor {
    explicit set_executor_visitor(std::shared_ptr<transwarp::executor> executor) noexcept
    : executor_{std::move(executor)} {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_executor(executor_);
    }

    std::shared_ptr<transwarp::executor> executor_;
};

/// Removes the executor from the given task
struct remove_executor_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.remove_executor();
    }
};

/// Assigns a priority to the given task
struct set_priority_visitor {
    explicit set_priority_visitor(std::int64_t priority) noexcept
    : priority_{priority} {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_priority(priority_);
    }

    std::int64_t priority_;
};

/// Resets the priority of the given task
struct reset_priority_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.reset_priority();
    }
};

/// Assigns custom data to the given task
struct set_custom_data_visitor {
    explicit set_custom_data_visitor(std::any custom_data) noexcept
    : custom_data_{std::move(custom_data)} {}

    void operator()(transwarp::itask& task) const noexcept {
        task.set_custom_data(custom_data_);
    }

    std::any custom_data_;
};

/// Removes custom data from the given task
struct remove_custom_data_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.remove_custom_data();
    }
};

/// Pushes the given task into the vector of tasks
struct push_task_visitor {
    explicit push_task_visitor(std::vector<transwarp::itask*>& tasks)
    : tasks_{tasks} {}

    void operator()(transwarp::itask& task) {
        tasks_.push_back(&task);
    }

    std::vector<transwarp::itask*>& tasks_;
};

/// Adds a new listener to the given task
struct add_listener_visitor {
    explicit add_listener_visitor(std::shared_ptr<transwarp::listener> listener)
    : listener_{std::move(listener)}
    {}

    void operator()(transwarp::itask& task) {
        task.add_listener(listener_);
    }

    std::shared_ptr<transwarp::listener> listener_;
};

/// Adds a new listener per event type to the given task
struct add_listener_per_event_visitor {
    add_listener_per_event_visitor(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener)
    : event_{event}, listener_{std::move(listener)}
    {}

    void operator()(transwarp::itask& task) {
        task.add_listener(event_, listener_);
    }

    transwarp::event_type event_;
    std::shared_ptr<transwarp::listener> listener_;
};

/// Removes a listener from the given task
struct remove_listener_visitor {
    explicit remove_listener_visitor(std::shared_ptr<transwarp::listener> listener)
    : listener_{std::move(listener)}
    {}

    void operator()(transwarp::itask& task) {
        task.remove_listener(listener_);
    }

    std::shared_ptr<transwarp::listener> listener_;
};

/// Removes a listener per event type from the given task
struct remove_listener_per_event_visitor {
    remove_listener_per_event_visitor(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener)
    : event_{event}, listener_{std::move(listener)}
    {}

    void operator()(transwarp::itask& task) {
        task.remove_listener(event_, listener_);
    }

    transwarp::event_type event_;
    std::shared_ptr<transwarp::listener> listener_;
};

/// Removes all listeners from the given task
struct remove_listeners_visitor {

    void operator()(transwarp::itask& task) {
        task.remove_listeners();
    }

};

/// Removes all listeners per event type from the given task
struct remove_listeners_per_event_visitor {
    explicit remove_listeners_per_event_visitor(transwarp::event_type event)
    : event_{event}
    {}

    void operator()(transwarp::itask& task) {
        task.remove_listeners(event_);
    }

    transwarp::event_type event_;
};

/// Visits the given task using the visitor given in the constructor
struct visit_visitor {
    explicit visit_visitor(const std::function<void(transwarp::itask&)>& visitor) noexcept
    : visitor_{visitor} {}

    void operator()(transwarp::itask& task) const {
        task.visit(visitor_);
    }

    const std::function<void(transwarp::itask&)>& visitor_;
};

/// Unvisits the given task
struct unvisit_visitor {

    void operator()(transwarp::itask& task) const noexcept {
        task.unvisit();
    }
};

/// Determines the result type of the Functor dispatching on the task type
template<typename TaskType, typename Functor, typename... ParentResults>
struct functor_result {
    static_assert(std::is_same_v<TaskType, transwarp::root_type> ||
                  std::is_same_v<TaskType, transwarp::accept_type> ||
                  std::is_same_v<TaskType, transwarp::accept_any_type> ||
                  std::is_same_v<TaskType, transwarp::consume_type> ||
                  std::is_same_v<TaskType, transwarp::consume_any_type> ||
                  std::is_same_v<TaskType, transwarp::wait_type> ||
                  std::is_same_v<TaskType, transwarp::wait_any_type>,
                  "Invalid task type, must be one of: root, accept, accept_any, consume, consume_any, wait, wait_any");
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::root_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) == 0, "A root task cannot have parent tasks");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::accept_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "An accept task must have at least one parent");
    using type = decltype(std::declval<Functor>()(std::declval<std::shared_future<ParentResults>>()...));
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::accept_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()(std::declval<std::vector<std::shared_future<ParentResultType>>>()));
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::accept_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "An accept_any task must have at least one parent");
    using arg_t = std::tuple_element_t<0, std::tuple<ParentResults...>>; // Using first type as reference
    using type = decltype(std::declval<Functor>()(std::declval<std::shared_future<arg_t>>()));
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::accept_any_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()(std::declval<std::shared_future<ParentResultType>>()));
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::consume_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A consume task must have at least one parent");
    using type = decltype(std::declval<Functor>()(std::declval<ParentResults>()...));
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::consume_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()(std::declval<std::vector<ParentResultType>>()));
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::consume_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A consume_any task must have at least one parent");
    using arg_t = std::tuple_element_t<0, std::tuple<ParentResults...>>; // Using first type as reference
    using type = decltype(std::declval<Functor>()(std::declval<arg_t>()));
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::consume_any_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()(std::declval<ParentResultType>()));
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::wait_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A wait task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::wait_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... ParentResults>
struct functor_result<transwarp::wait_any_type, Functor, ParentResults...> {
    static_assert(sizeof...(ParentResults) > 0, "A wait_any task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename ParentResultType>
struct functor_result<transwarp::wait_any_type, Functor, std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = decltype(std::declval<Functor>()());
};

/// Determines the result type of the Functor dispatching on the task type
template<typename TaskType, typename Functor, typename... ParentResults>
using functor_result_t = typename transwarp::detail::functor_result<TaskType, Functor, ParentResults...>::type;


/// Assigns the node to the given functor if the functor is a subclass of transwarp::functor
template<typename Functor>
void assign_node_if(Functor& functor, const std::shared_ptr<transwarp::node>& node) noexcept {
    if constexpr (std::is_base_of_v<transwarp::functor, Functor>) {
        functor.transwarp_node_ = node;
    }
}


/// Returns a ready future with the given value as its state
template<typename ResultType, typename Value>
std::shared_future<ResultType> make_future_with_value(Value&& value) {
    std::promise<ResultType> promise;
    promise.set_value(std::forward<Value>(value));
    return promise.get_future();
}

/// Returns a ready future
inline std::shared_future<void> make_ready_future() {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
}

/// Returns a ready future with the given exception as its state
template<typename ResultType>
std::shared_future<ResultType> make_future_with_exception(std::exception_ptr exception) {
    if (!exception) {
        throw transwarp::invalid_parameter{"exception pointer"};
    }
    std::promise<ResultType> promise;
    promise.set_exception(exception);
    return promise.get_future();
}


/// Determines the type of the parents
template<typename... ParentResults>
struct parents {
    using type = std::tuple<std::shared_ptr<transwarp::task<ParentResults>>...>;
    static std::size_t size(const type&) {
        return std::tuple_size_v<type>;
    }
    static type clone(transwarp::node& node, std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const type& obj) {
        type cloned = obj;
        transwarp::detail::apply_to_each(
            [&node,&task_cache](auto& t) {
                t = clone_task(task_cache, t);
                transwarp::detail::node_manip::add_parent(node, t->node());
            }, cloned);
        return cloned;
    }
};

/// Determines the type of the parents. Specialization for vector parents
template<typename ParentResultType>
struct parents<std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>> {
    using type = std::vector<std::shared_ptr<transwarp::task<ParentResultType>>>;
    static std::size_t size(const type& obj) {
        return obj.size();
    }
    static type clone(transwarp::node& node, std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache, const type& obj) {
        type cloned = obj;
        for (auto& t : cloned) {
            t = clone_task(task_cache, t);
            transwarp::detail::node_manip::add_parent(node, t->node());
        }
        return cloned;
    }
};

/// Determines the type of the parents
template<typename... ParentResults>
using parents_t = typename transwarp::detail::parents<ParentResults...>::type;


template<typename ResultType, typename TaskType>
class base_runner {
protected:

    template<typename Task, typename Parents>
    void call(std::size_t node_id,
              const std::weak_ptr<Task>& task,
              const Parents& parents) {
        promise_.set_value(transwarp::detail::call<TaskType, ResultType>(node_id, task, parents));
    }

    std::promise<ResultType> promise_;
};

template<typename TaskType>
class base_runner<void, TaskType> {
protected:

    template<typename Task, typename Parents>
    void call(std::size_t node_id,
              const std::weak_ptr<Task>& task,
              const Parents& parents) {
        transwarp::detail::call<TaskType, void>(node_id, task, parents);
        promise_.set_value();
    }

    std::promise<void> promise_;
};

/// A callable to run a task given its parents
template<typename ResultType, typename TaskType, typename Task, typename Parents>
class runner : public transwarp::detail::base_runner<ResultType, TaskType> {
public:

    runner(std::size_t node_id,
           const std::weak_ptr<Task>& task,
           const transwarp::decay_t<Parents>& parents)
    : node_id_{node_id},
      task_{task},
      parents_{parents}
    {}

    std::future<ResultType> future() {
        return this->promise_.get_future();
    }

    void operator()() {
        if (const std::shared_ptr<Task> t = task_.lock()) {
            t->raise_event(transwarp::event_type::before_started);
        }
        try {
            this->call(node_id_, task_, parents_);
        } catch (const transwarp::task_canceled&) {
            this->promise_.set_exception(std::current_exception());
            if (const std::shared_ptr<Task> t = task_.lock()) {
                t->raise_event(transwarp::event_type::after_canceled);
            }
        } catch (...) {
            this->promise_.set_exception(std::current_exception());
        }
        if (const std::shared_ptr<Task> t = task_.lock()) {
            t->raise_event(transwarp::event_type::after_finished);
        }
    }

private:
    const std::size_t node_id_;
    const std::weak_ptr<Task> task_;
    const transwarp::decay_t<Parents> parents_;
};


/// A simple circular buffer (FIFO).
/// ValueType must support default construction. The buffer lets you push
/// new values onto the back and pop old values off the front.
template<typename ValueType>
class circular_buffer {
public:

    static_assert(std::is_default_constructible_v<ValueType>, "ValueType must be default constructible");

    using value_type = ValueType;

    /// Constructs a circular buffer with a given fixed capacity
    explicit
    circular_buffer(std::size_t capacity)
    : data_(capacity)
    {
        if (capacity < 1) {
            throw transwarp::invalid_parameter{"capacity"};
        }
    }

    // delete copy/move semantics
    circular_buffer(const circular_buffer&) = delete;
    circular_buffer& operator=(const circular_buffer&) = delete;
    circular_buffer(circular_buffer&& other) = delete;
    circular_buffer& operator=(circular_buffer&&) = delete;

    /// Pushes a new value onto the end of the buffer. If that exceeds the capacity
    /// of the buffer then the oldest value gets dropped (the one at the front).
    template<typename T, typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, value_type>>>
    void push(T&& value) {
        data_[end_] = std::forward<T>(value);
        increment();
    }

    /// Returns the value at the front of the buffer (the oldest value).
    /// This is undefined if the buffer is empty
    const value_type& front() const {
        return data_[front_];
    }

    /// Removes the value at the front of the buffer (the oldest value)
    void pop() {
        if (!empty()) {
            data_[front_] = ValueType{};
            decrement();
        }
    }

    /// Returns the capacity of the buffer
    std::size_t capacity() const {
        return data_.size();
    }

    /// Returns the number of populated values of the buffer. Its maximum value
    /// equals the capacity of the buffer
    std::size_t size() const {
        return size_;
    }

    /// Returns whether the buffer is empty
    bool empty() const {
        return size_ == 0;
    }

    /// Returns whether the buffer is full
    bool full() const {
        return size_ == data_.size();
    }

    /// Swaps this buffer with the given buffer
    void swap(circular_buffer& buffer) {
        std::swap(end_, buffer.end_);
        std::swap(front_, buffer.front_);
        std::swap(size_, buffer.size_);
        std::swap(data_, buffer.data_);
    }

private:

    void increment_or_wrap(std::size_t& value) const {
        if (value == data_.size() - 1) {
            value = 0;
        } else {
            ++value;
        }
    }

    void increment() {
        increment_or_wrap(end_);
        if (full()) {
            increment_or_wrap(front_);
        } else {
            ++size_;
        }
    }

    void decrement() {
        increment_or_wrap(front_);
        --size_;
    }

    std::size_t end_{};
    std::size_t front_{};
    std::size_t size_{};
    std::vector<value_type> data_;
};


class spinlock {
public:

    void lock() noexcept {
        while (locked_.test_and_set(std::memory_order_acquire));
    }

    void unlock() noexcept {
        locked_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag locked_ = ATOMIC_FLAG_INIT;
};


} // detail


/// A functor not doing nothing
struct no_op_functor {
    void operator()() const noexcept {}
};

/// An object to use in places where a no-op functor is required
constexpr no_op_functor no_op{};


/// Executor for sequential execution. Runs functors sequentially on the same thread
class sequential : public transwarp::executor {
public:

    sequential() = default;

    // delete copy/move semantics
    sequential(const sequential&) = delete;
    sequential& operator=(const sequential&) = delete;
    sequential(sequential&&) = delete;
    sequential& operator=(sequential&&) = delete;

    /// Returns the name of the executor
    std::string name() const override {
        return "transwarp::sequential";
    }

    /// Runs the functor on the current thread
    void execute(const std::function<void()>& functor, const transwarp::itask&) override {
        functor();
    }
};


/// Executor for parallel execution. Uses a simple thread pool
class parallel : public transwarp::executor {
public:

    explicit parallel(std::size_t n_threads)
    : pool_{n_threads}
    {}

    // delete copy/move semantics
    parallel(const parallel&) = delete;
    parallel& operator=(const parallel&) = delete;
    parallel(parallel&&) = delete;
    parallel& operator=(parallel&&) = delete;

    /// Returns the name of the executor
    std::string name() const override {
        return "transwarp::parallel";
    }

    /// Pushes the functor into the thread pool for asynchronous execution
    void execute(const std::function<void()>& functor, const transwarp::itask&) override {
        pool_.push(functor);
    }

private:
    transwarp::detail::thread_pool pool_;
};


/// Detail namespace for internal functionality only
namespace detail {

/// The base task class that contains the functionality that can be used
/// with all result types (void and non-void).
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_base : public transwarp::task<ResultType>,
                       public std::enable_shared_from_this<task_impl_base<ResultType, TaskType, Functor, ParentResults...>> {
public:
    /// The task type
    using task_type = TaskType;

    /// The result type of this task
    using result_type = ResultType;

    /// Can be called to explicitly finalize this task making this task
    /// the terminal task in the graph. This is also done implicitly when
    /// calling, e.g., any of the *_all methods. It should normally not be
    /// necessary to call this method directly
    void finalize() override {
        if (tasks_.empty()) {
            visit(transwarp::detail::final_visitor{tasks_});
            unvisit();
            auto compare = [](const transwarp::itask* const l, const transwarp::itask* const r) {
                const std::size_t l_level = l->node()->level();
                const std::size_t l_id = l->node()->id();
                const std::size_t r_level = r->node()->level();
                const std::size_t r_id = r->node()->id();
                return std::tie(l_level, l_id) < std::tie(r_level, r_id);
            };
            std::sort(tasks_.begin(), tasks_.end(), compare);
        }
    }

    /// Assigns an executor to this task which takes precedence over
    /// the executor provided in schedule() or schedule_all()
    void set_executor(std::shared_ptr<transwarp::executor> executor) override {
        ensure_task_not_running();
        if (!executor) {
            throw transwarp::invalid_parameter("executor pointer");
        }
        executor_ = std::move(executor);
        transwarp::detail::node_manip::set_executor(*node_, std::make_optional(executor_->name()));
    }

    /// Assigns an executor to all tasks which takes precedence over
    /// the executor provided in schedule() or schedule_all()
    void set_executor_all(std::shared_ptr<transwarp::executor> executor) override {
        ensure_task_not_running();
        transwarp::detail::set_executor_visitor visitor{std::move(executor)};
        visit_all(visitor);
    }

    /// Removes the executor from this task
    void remove_executor() override {
        ensure_task_not_running();
        executor_.reset();
        transwarp::detail::node_manip::set_executor(*node_, {});
    }

    /// Removes the executor from all tasks
    void remove_executor_all() override {
        ensure_task_not_running();
        transwarp::detail::remove_executor_visitor visitor;
        visit_all(visitor);
    }

    /// Sets a task priority (defaults to 0). transwarp will not directly use this.
    /// This is only useful if something else is using the priority (e.g. a custom executor)
    void set_priority(std::int64_t priority) override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_priority(*node_, priority);
    }

    /// Sets a priority to all tasks (defaults to 0). transwarp will not directly use this.
    /// This is only useful if something else is using the priority (e.g. a custom executor)
    void set_priority_all(std::int64_t priority) override {
        ensure_task_not_running();
        transwarp::detail::set_priority_visitor visitor{priority};
        visit_all(visitor);
    }

    /// Resets the task priority to 0
    void reset_priority() override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_priority(*node_, 0);
    }

    /// Resets the priority of all tasks to 0
    void reset_priority_all() override {
        ensure_task_not_running();
        transwarp::detail::reset_priority_visitor visitor;
        visit_all(visitor);
    }

    /// Assigns custom data to this task. transwarp will not directly use this.
    /// This is only useful if something else is using this custom data (e.g. a custom executor)
    void set_custom_data(std::any custom_data) override {
        ensure_task_not_running();
        if (!custom_data.has_value()) {
            throw transwarp::invalid_parameter{"custom data"};
        }
        transwarp::detail::node_manip::set_custom_data(*node_, std::move(custom_data));
    }

    /// Assigns custom data to all tasks. transwarp will not directly use this.
    /// This is only useful if something else is using this custom data (e.g. a custom executor)
    void set_custom_data_all(std::any custom_data) override {
        ensure_task_not_running();
        transwarp::detail::set_custom_data_visitor visitor{std::move(custom_data)};
        visit_all(visitor);
    }

    /// Removes custom data from this task
    void remove_custom_data() override {
        ensure_task_not_running();
        transwarp::detail::node_manip::set_custom_data(*node_, {});
    }

    /// Removes custom data from all tasks
    void remove_custom_data_all() override {
        ensure_task_not_running();
        transwarp::detail::remove_custom_data_visitor visitor;
        visit_all(visitor);
    }

    /// Returns the future associated to the underlying execution
    const std::shared_future<result_type>& future() const noexcept override {
        return future_;
    }

    /// Returns the associated node
    const std::shared_ptr<transwarp::node>& node() const noexcept override {
        return node_;
    }

    /// Adds a new listener for all event types
    void add_listener(std::shared_ptr<transwarp::listener> listener) override {
        ensure_task_not_running();
        check_listener(listener);
        for (std::vector<std::shared_ptr<transwarp::listener>>& l : listeners_) {
            l.push_back(listener);
        }
    }

    /// Adds a new listener for the given event type only
    void add_listener(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener) override {
        ensure_task_not_running();
        check_listener(listener);
        listeners_[event_index(event)].push_back(std::move(listener));
    }

    /// Adds a new listener for all event types and for all parents
    void add_listener_all(std::shared_ptr<transwarp::listener> listener) override {
        ensure_task_not_running();
        transwarp::detail::add_listener_visitor visitor{std::move(listener)};
        visit_all(visitor);
    }

    /// Adds a new listener for the given event type only and for all parents
    void add_listener_all(transwarp::event_type event, std::shared_ptr<transwarp::listener> listener) override {
        ensure_task_not_running();
        transwarp::detail::add_listener_per_event_visitor visitor{event, std::move(listener)};
        visit_all(visitor);
    }

    /// Removes the listener for all event types
    void remove_listener(const std::shared_ptr<transwarp::listener>& listener) override {
        ensure_task_not_running();
        check_listener(listener);
        for (std::vector<std::shared_ptr<transwarp::listener>>& l : listeners_) {
            l.erase(std::remove(l.begin(), l.end(), listener), l.end());
        }
    }

    /// Removes the listener for the given event type only
    void remove_listener(transwarp::event_type event, const std::shared_ptr<transwarp::listener>& listener) override {
        ensure_task_not_running();
        check_listener(listener);
        std::vector<std::shared_ptr<transwarp::listener>>& l = listeners_[event_index(event)];
        l.erase(std::remove(l.begin(), l.end(), listener), l.end());
    }

    /// Removes the listener for all event types and for all parents
    void remove_listener_all(const std::shared_ptr<transwarp::listener>& listener) override {
        ensure_task_not_running();
        transwarp::detail::remove_listener_visitor visitor{std::move(listener)};
        visit_all(visitor);
    }

    /// Removes the listener for the given event type only and for all parents
    void remove_listener_all(transwarp::event_type event, const std::shared_ptr<transwarp::listener>& listener) override {
        ensure_task_not_running();
        transwarp::detail::remove_listener_per_event_visitor visitor{event, std::move(listener)};
        visit_all(visitor);
    }

    /// Removes all listeners
    void remove_listeners() override {
        ensure_task_not_running();
        for (std::vector<std::shared_ptr<transwarp::listener>>& l : listeners_) {
            l.clear();
        }
    }

    /// Removes all listeners for the given event type
    void remove_listeners(transwarp::event_type event) override {
        ensure_task_not_running();
        listeners_[event_index(event)].clear();
    }

    /// Removes all listeners and for all parents
    void remove_listeners_all() override {
        ensure_task_not_running();
        transwarp::detail::remove_listeners_visitor visitor;
        visit_all(visitor);
    }

    /// Removes all listeners for the given event type and for all parents
    void remove_listeners_all(transwarp::event_type event) override {
        ensure_task_not_running();
        transwarp::detail::remove_listeners_per_event_visitor visitor{event};
        visit_all(visitor);
    }

    /// Schedules this task for execution on the caller thread.
    /// The task-specific executor gets precedence if it exists.
    /// This overload will reset the underlying future.
    void schedule() override {
        ensure_task_not_running();
        this->schedule_impl(true);
    }

    /// Schedules this task for execution on the caller thread.
    /// The task-specific executor gets precedence if it exists.
    /// reset denotes whether schedule should reset the underlying
    /// future and schedule even if the future is already valid.
    void schedule(bool reset) override {
        ensure_task_not_running();
        this->schedule_impl(reset);
    }

    /// Schedules this task for execution using the provided executor.
    /// The task-specific executor gets precedence if it exists.
    /// This overload will reset the underlying future.
    void schedule(transwarp::executor& executor) override {
        ensure_task_not_running();
        this->schedule_impl(true, &executor);
    }

    /// Schedules this task for execution using the provided executor.
    /// The task-specific executor gets precedence if it exists.
    /// reset denotes whether schedule should reset the underlying
    /// future and schedule even if the future is already valid.
    void schedule(transwarp::executor& executor, bool reset) override {
        ensure_task_not_running();
        this->schedule_impl(reset, &executor);
    }

    /// Schedules all tasks in the graph for execution on the caller thread.
    /// The task-specific executors get precedence if they exist.
    /// This overload will reset the underlying futures.
    void schedule_all() override {
        ensure_task_not_running();
        schedule_all_impl(true);
    }

    /// Schedules all tasks in the graph for execution using the provided executor.
    /// The task-specific executors get precedence if they exist.
    /// This overload will reset the underlying futures.
    void schedule_all(transwarp::executor& executor) override {
        ensure_task_not_running();
        schedule_all_impl(true, &executor);
    }

    /// Schedules all tasks in the graph for execution on the caller thread.
    /// The task-specific executors get precedence if they exist.
    /// reset_all denotes whether schedule_all should reset the underlying
    /// futures and schedule even if the futures are already present.
    void schedule_all(bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all);
    }

    /// Schedules all tasks in the graph for execution using the provided executor.
    /// The task-specific executors get precedence if they exist.
    /// reset_all denotes whether schedule_all should reset the underlying
    /// futures and schedule even if the futures are already present.
    void schedule_all(transwarp::executor& executor, bool reset_all) override {
        ensure_task_not_running();
        schedule_all_impl(reset_all, &executor);
    }

    /// Assigns an exception to this task. Scheduling will have no effect after an exception
    /// has been set. Calling reset() will remove the exception and re-enable scheduling.
    void set_exception(std::exception_ptr exception) override {
        ensure_task_not_running();
        future_ = transwarp::detail::make_future_with_exception<result_type>(exception);
        schedule_mode_ = false;
    }

    /// Returns whether the task was scheduled and not reset afterwards.
    /// This means that the underlying future is valid
    bool was_scheduled() const noexcept override {
        return future_.valid();
    }

    /// Waits for the task to complete. Should only be called if was_scheduled()
    /// is true, throws transwarp::control_error otherwise
    void wait() const override {
        ensure_task_was_scheduled();
        future_.wait();
    }

    /// Returns whether the task has finished processing. Should only be called
    /// if was_scheduled() is true, throws transwarp::control_error otherwise
    bool is_ready() const override {
        ensure_task_was_scheduled();
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    /// Returns whether this task contains a result
    bool has_result() const noexcept override {
        return was_scheduled() && future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    /// Resets this task
    void reset() override {
        ensure_task_not_running();
        future_ = std::shared_future<result_type>{};
        transwarp::detail::node_manip::set_canceled(*node_, false);
        schedule_mode_ = true;
    }

    /// Resets all tasks in the graph
    void reset_all() override {
        ensure_task_not_running();
        transwarp::detail::reset_visitor visitor;
        visit_all(visitor);
    }

    /// If enabled then this task is canceled which will
    /// throw transwarp::task_canceled when retrieving the task result.
    /// Passing false is equivalent to resume.
    void cancel(bool enabled) noexcept override {
        transwarp::detail::node_manip::set_canceled(*node_, enabled);
    }

    /// If enabled then all pending tasks in the graph are canceled which will
    /// throw transwarp::task_canceled when retrieving the task result.
    /// Passing false is equivalent to resume.
    void cancel_all(bool enabled) noexcept override {
        transwarp::detail::cancel_visitor visitor{enabled};
        visit_all(visitor);
    }

    /// Returns all tasks in the graph in breadth order
    const std::vector<transwarp::itask*>& tasks() override {
        finalize();
        return tasks_;
    }

    /// Returns all edges in the graph. This is mainly for visualizing
    /// the tasks and their interdependencies. Pass the result into transwarp::to_string
    /// to retrieve a dot-style graph representation for easy viewing.
    std::vector<transwarp::edge> edges() override {
        std::vector<transwarp::edge> edges;
        transwarp::detail::edges_visitor visitor{edges};
        visit_all(visitor);
        return edges;
    }

protected:

    task_impl_base() = default;

    template<typename F>
    task_impl_base(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : node_{new transwarp::node},
      functor_{new Functor{std::forward<F>(functor)}},
      parents_{std::move(parents)...}
    {
        init();
    }

    template<typename F, typename P>
    task_impl_base(F&& functor, std::vector<std::shared_ptr<transwarp::task<P>>> parents)
    : node_{new transwarp::node},
      functor_{new Functor{std::forward<F>(functor)}},
      parents_{std::move(parents)}
    {
        if (parents_.empty()) {
            throw transwarp::invalid_parameter{"parents are empty"};
        }
        init();
    }

    void init() {
        transwarp::detail::node_manip::set_type(*node_, task_type::value);
        transwarp::detail::assign_node_if(*functor_, node_);
        transwarp::detail::call_with_each(transwarp::detail::parent_visitor{*node_}, parents_);
    }

    /// Checks if the task is currently running and throws transwarp::control_error if it is
    void ensure_task_not_running() const {
        if (future_.valid() && future_.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
            throw transwarp::control_error("task currently running: " + transwarp::to_string(*node_, " "));
        }
    }

    /// Checks if the task was scheduled and throws transwarp::control_error if it's not
    void ensure_task_was_scheduled() const {
        if (!future_.valid()) {
            throw transwarp::control_error{"task was not scheduled: " + transwarp::to_string(*node_, " ")};
        }
    }

    template<typename R, typename Y, typename T, typename P>
    friend class transwarp::detail::runner;

    template<typename R, typename T, typename... A>
    friend R transwarp::detail::run_task(std::size_t, const std::weak_ptr<T>&, A&&...);

    /// Assigns the given id to the node
    void set_node_id(std::size_t id) noexcept override {
        transwarp::detail::node_manip::set_id(*node_, id);
    }

    /// Schedules this task for execution using the provided executor.
    /// The task-specific executor gets precedence if it exists.
    /// Runs the task on the same thread as the caller if neither the global
    /// nor the task-specific executor is found.
    void schedule_impl(bool reset, transwarp::executor* executor=nullptr) override {
        if (schedule_mode_ && (reset || !future_.valid())) {
            if (reset) {
                transwarp::detail::node_manip::set_canceled(*node_, false);
            }
            std::weak_ptr<task_impl_base> self = this->shared_from_this();
            using runner_t = transwarp::detail::runner<result_type, task_type, task_impl_base, decltype(parents_)>;
            std::shared_ptr<runner_t> runner = std::shared_ptr<runner_t>{new runner_t{node_->id(), self, parents_}};
            raise_event(transwarp::event_type::before_scheduled);
            future_ = runner->future();
            if (executor_) {
                executor_->execute([runner]{ (*runner)(); }, *this);
            } else if (executor) {
                executor->execute([runner]{ (*runner)(); }, *this);
            } else {
                (*runner)();
            }
        }
    }

    /// Schedules all tasks in the graph for execution using the provided executor.
    /// The task-specific executors get precedence if they exist.
    /// Runs tasks on the same thread as the caller if neither the global
    /// nor a task-specific executor is found.
    void schedule_all_impl(bool reset_all, transwarp::executor* executor=nullptr) {
        transwarp::detail::schedule_visitor visitor{reset_all, executor};
        visit_all(visitor);
    }

    /// Visits each task in a depth-first traversal
    void visit(const std::function<void(transwarp::itask&)>& visitor) override {
        if (!visited_) {
            transwarp::detail::call_with_each(transwarp::detail::visit_visitor{visitor}, parents_);
            visitor(*this);
            visited_ = true;
        }
    }

    /// Traverses through each task and marks them as not visited.
    void unvisit() noexcept override {
        if (visited_) {
            visited_ = false;
            transwarp::detail::call_with_each(transwarp::detail::unvisit_visitor{}, parents_);
        }
    }

    /// Visits all tasks
    template<typename Visitor>
    void visit_all(Visitor& visitor) {
        finalize();
        for (transwarp::itask* t : tasks_) {
            visitor(*t);
        }
    }

    /// Returns the index for a given event type
    std::size_t event_index(transwarp::event_type event) const {
        const std::size_t index = static_cast<std::size_t>(event);
        if (index >= static_cast<std::size_t>(transwarp::event_type::count)) {
            throw transwarp::invalid_parameter{"event type"};
        }
        return index;
    }

    /// Raises the given event to all listeners
    void raise_event(transwarp::event_type event) const {
        for (const std::shared_ptr<transwarp::listener>& listener : listeners_[static_cast<std::size_t>(event)]) {
            listener->handle_event(event, *this);
        }
    }

    /// Check for non-null listener pointer
    void check_listener(const std::shared_ptr<transwarp::listener>& listener) const {
        if (!listener) {
            throw transwarp::invalid_parameter{"listener pointer"};
        }
    }

    bool schedule_mode_ = true;
    std::shared_future<result_type> future_;
    std::shared_ptr<transwarp::node> node_;
    std::unique_ptr<Functor> functor_;
    transwarp::detail::parents_t<ParentResults...> parents_;
    bool visited_ = false;
    std::shared_ptr<transwarp::executor> executor_;
    std::array<std::vector<std::shared_ptr<transwarp::listener>>, static_cast<std::size_t>(transwarp::event_type::count)> listeners_;
    std::vector<transwarp::itask*> tasks_;
};


/// A task proxy
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy : public transwarp::detail::task_impl_base<ResultType, TaskType, Functor, ParentResults...> {
public:
    /// The task type
    using task_type = TaskType;

    /// The result type of this task
    using result_type = ResultType;

    /// Assigns a value to this task. Scheduling will have no effect after a value
    /// has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(const transwarp::decay_t<result_type>& value) override {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_future_with_value<result_type>(value);
        this->schedule_mode_ = false;
    }

    /// Assigns a value to this task. Scheduling will have no effect after a value
    /// has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(transwarp::decay_t<result_type>&& value) override {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_future_with_value<result_type>(std::move(value));
        this->schedule_mode_ = false;
    }

    /// Returns the result of this task. Throws any exceptions that the underlying
    /// functor throws. Should only be called if was_scheduled() is true,
    /// throws transwarp::control_error otherwise
    transwarp::result_t<result_type> get() const override {
        this->ensure_task_was_scheduled();
        return this->future_.get();
    }

protected:

    task_impl_proxy() = default;

    template<typename F>
    task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)...}
    {}

    template<typename F, typename P>
    task_impl_proxy(F&& functor, std::vector<std::shared_ptr<transwarp::task<P>>> parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)}
    {}

};

/// A task proxy for reference result type.
template<typename ResultType, typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy<ResultType&, TaskType, Functor, ParentResults...> : public transwarp::detail::task_impl_base<ResultType&, TaskType, Functor, ParentResults...> {
public:
    /// The task type
    using task_type = TaskType;

    /// The result type of this task
    using result_type = ResultType&;

    /// Assigns a value to this task. Scheduling will have no effect after a value
    /// has been set. Calling reset() will remove the value and re-enable scheduling.
    void set_value(transwarp::decay_t<result_type>& value) override {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_future_with_value<result_type>(value);
        this->schedule_mode_ = false;
    }

    /// Returns the result of this task. Throws any exceptions that the underlying
    /// functor throws. Should only be called if was_scheduled() is true,
    /// throws transwarp::control_error otherwise
    transwarp::result_t<result_type> get() const override {
        this->ensure_task_was_scheduled();
        return this->future_.get();
    }

protected:

    task_impl_proxy() = default;

    template<typename F>
    task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)...}
    {}

    template<typename F, typename P>
    task_impl_proxy(F&& functor, std::vector<std::shared_ptr<transwarp::task<P>>> parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)}
    {}

};

/// A task proxy for void result type.
template<typename TaskType, typename Functor, typename... ParentResults>
class task_impl_proxy<void, TaskType, Functor, ParentResults...> : public transwarp::detail::task_impl_base<void, TaskType, Functor, ParentResults...> {
public:
    /// The task type
    using task_type = TaskType;

    /// The result type of this task
    using result_type = void;

    /// Assigns a value to this task. Scheduling will have no effect after a call
    /// to this. Calling reset() will reset this and re-enable scheduling.
    void set_value() override {
        this->ensure_task_not_running();
        this->future_ = transwarp::detail::make_ready_future();
        this->schedule_mode_ = false;
    }

    /// Blocks until the task finishes. Throws any exceptions that the underlying
    /// functor throws. Should only be called if was_scheduled() is true,
    /// throws transwarp::control_error otherwise
    void get() const override {
        this->ensure_task_was_scheduled();
        this->future_.get();
    }

protected:

    task_impl_proxy() = default;

    template<typename F>
    task_impl_proxy(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)...}
    {}

    template<typename F, typename P>
    task_impl_proxy(F&& functor, std::vector<std::shared_ptr<transwarp::task<P>>> parents)
    : transwarp::detail::task_impl_base<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)}
    {}

};

} // detail


/// A task representing a piece of work given by functor and parent tasks.
/// By connecting tasks a directed acyclic graph is built.
/// Tasks should be created using the make_task factory functions.
template<typename TaskType, typename Functor, typename... ParentResults>
class task_impl : public transwarp::detail::task_impl_proxy<transwarp::detail::functor_result_t<TaskType, Functor, ParentResults...>, TaskType, Functor, ParentResults...> {
public:
    /// The task type
    using task_type = TaskType;

    /// The result type of this task
    using result_type = transwarp::detail::functor_result_t<TaskType, Functor, ParentResults...>;

    /// A task is defined functor and parent tasks.
    /// Note: Don't use this constructor directly, use transwarp::make_task
    template<typename F>
    task_impl(F&& functor, std::shared_ptr<transwarp::task<ParentResults>>... parents)
    : transwarp::detail::task_impl_proxy<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)...}
    {}

    /// A task is defined functor and parent tasks.
    /// Note: Don't use this constructor directly, use transwarp::make_task
    template<typename F, typename P>
    task_impl(F&& functor, std::vector<std::shared_ptr<transwarp::task<P>>> parents)
    : transwarp::detail::task_impl_proxy<result_type, task_type, Functor, ParentResults...>{std::forward<F>(functor), std::move(parents)}
    {}

    // delete copy/move semantics
    task_impl(const task_impl&) = delete;
    task_impl& operator=(const task_impl&) = delete;
    task_impl(task_impl&&) = delete;
    task_impl& operator=(task_impl&&) = delete;

    /// Gives this task a name and returns a ptr to itself
    std::shared_ptr<task_impl> named(std::string name) {
        transwarp::detail::node_manip::set_name(*this->node(), std::make_optional(std::move(name)));
        return std::dynamic_pointer_cast<task_impl>(this->shared_from_this());
    }

    /// Creates a continuation to this task
    template<typename TaskType_, typename Functor_>
    auto then(TaskType_, Functor_&& functor) {
        using task_t = transwarp::task_impl<TaskType_, std::decay_t<Functor_>, result_type>;
        return std::shared_ptr<task_t>{new task_t{std::forward<Functor_>(functor), this->shared_from_this()}};
    }

    /// Clones this task and casts the result to a ptr to task_impl
    std::shared_ptr<task_impl> clone_cast() const {
        return std::dynamic_pointer_cast<task_impl>(this->clone());
    }

private:

    task_impl() = default;

    std::shared_ptr<transwarp::task<result_type>> clone_impl(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>& task_cache) const override {
        auto t = std::shared_ptr<task_impl>{new task_impl};
        t->schedule_mode_ = this->schedule_mode_;
        if (this->has_result()) {
            try {
                if constexpr (std::is_void_v<result_type>) {
                    this->future_.get();
                    t->future_ = transwarp::detail::make_ready_future();
                } else {
                    t->future_ = transwarp::detail::make_future_with_value<result_type>(this->future_.get());
                }
            } catch (...) {
                t->future_ = transwarp::detail::make_future_with_exception<result_type>(std::current_exception());
            }
        }
        t->node_ = this->node_->clone();
        t->functor_ = std::unique_ptr<Functor>{new Functor{*this->functor_}};
        t->parents_ = transwarp::detail::parents<ParentResults...>::clone(*(t->node_), task_cache, this->parents_);
        t->visited_ = this->visited_;
        t->executor_ = this->executor_;
        t->listeners_ = this->listeners_;
        return t;
    }

};


/// A value task that stores a single value and doesn't require scheduling.
/// Value tasks should be created using the make_value_task factory functions.
template<typename ResultType>
class value_task : public transwarp::task<ResultType>,
                   public std::enable_shared_from_this<value_task<ResultType>> {
public:
    /// The task type
    using task_type = transwarp::root_type;

    /// The result type of this task
    using result_type = ResultType;

    /// A value task is defined a value.
    /// Note: Don't use this constructor directly, use transwarp::make_value_task
    template<typename T>
    value_task(T&& value)
    : node_{new transwarp::node},
      future_{transwarp::detail::make_future_with_value<result_type>(std::forward<T>(value))}
    {
        transwarp::detail::node_manip::set_type(*node_, task_type::value);
    }

    // delete copy/move semantics
    value_task(const value_task&) = delete;
    value_task& operator=(const value_task&) = delete;
    value_task(value_task&&) = delete;
    value_task& operator=(value_task&&) = delete;

    /// Gives this task a name and returns a ptr to itself
    std::shared_ptr<value_task> named(std::string name) {
        transwarp::detail::node_manip::set_name(*node_, std::make_optional(std::move(name)));
        return this->shared_from_this();
    }

    /// Creates a continuation to this task
    template<typename TaskType_, typename Functor_>
    auto then(TaskType_, Functor_&& functor) {
        using task_t = transwarp::task_impl<TaskType_, std::decay_t<Functor_>, result_type>;
        return std::shared_ptr<task_t>{new task_t{std::forward<Functor_>(functor), this->shared_from_this()}};
    }

    /// Clones this task and casts the result to a ptr to value_task
    std::shared_ptr<value_task> clone_cast() const {
        return std::dynamic_pointer_cast<value_task>(this->clone());
    }

    /// Nothing to be done to finalize a value task
    void finalize() override {}

    /// No-op because a value task never runs
    void set_executor(std::shared_ptr<transwarp::executor>) override {}

    /// No-op because a value task never runs and doesn't have parents
    void set_executor_all(std::shared_ptr<transwarp::executor>) override {}

    /// No-op because a value task never runs
    void remove_executor() override {}

    /// No-op because a value task never runs and doesn't have parents
    void remove_executor_all() override {}

    /// Sets a task priority (defaults to 0). transwarp will not directly use this.
    /// This is only useful if something else is using the priority
    void set_priority(std::int64_t priority) override {
        transwarp::detail::node_manip::set_priority(*node_, priority);
    }

    /// Sets a priority to all tasks (defaults to 0). transwarp will not directly use this.
    /// This is only useful if something else is using the priority
    void set_priority_all(std::int64_t priority) override {
        set_priority(priority);
    }

    /// Resets the task priority to 0
    void reset_priority() override {
        transwarp::detail::node_manip::set_priority(*node_, 0);
    }

    /// Resets the priority of all tasks to 0
    void reset_priority_all() override {
        reset_priority();
    }

    /// Assigns custom data to this task. transwarp will not directly use this.
    /// This is only useful if something else is using this custom data
    void set_custom_data(std::any custom_data) override {
        if (!custom_data.has_value()) {
            throw transwarp::invalid_parameter{"custom data"};
        }
        transwarp::detail::node_manip::set_custom_data(*node_, std::move(custom_data));
    }

    /// Assigns custom data to all tasks. transwarp will not directly use this.
    /// This is only useful if something else is using this custom data
    void set_custom_data_all(std::any custom_data) override {
        set_custom_data(std::move(custom_data));
    }

    /// Removes custom data from this task
    void remove_custom_data() override {
        transwarp::detail::node_manip::set_custom_data(*node_, {});
    }

    /// Removes custom data from all tasks
    void remove_custom_data_all() override {
        remove_custom_data();
    }

    /// Returns the future associated to the underlying execution
    const std::shared_future<result_type>& future() const noexcept override {
        return future_;
    }

    /// Returns the associated node
    const std::shared_ptr<transwarp::node>& node() const noexcept override {
        return node_;
    }

    /// No-op because a value task doesn't raise events
    void add_listener(std::shared_ptr<transwarp::listener>) override {}

    /// No-op because a value task doesn't raise events
    void add_listener(transwarp::event_type, std::shared_ptr<transwarp::listener>) override {}

    /// No-op because a value task doesn't raise events
    void add_listener_all(std::shared_ptr<transwarp::listener>) override {}

    /// No-op because a value task doesn't raise events
    void add_listener_all(transwarp::event_type, std::shared_ptr<transwarp::listener>) override {}

    /// No-op because a value task doesn't raise events
    void remove_listener(const std::shared_ptr<transwarp::listener>&) override {}

    /// No-op because a value task doesn't raise events
    void remove_listener(transwarp::event_type, const std::shared_ptr<transwarp::listener>&) override {}

    /// No-op because a value task doesn't raise events
    void remove_listener_all(const std::shared_ptr<transwarp::listener>&) override {}

    /// No-op because a value task doesn't raise events
    void remove_listener_all(transwarp::event_type, const std::shared_ptr<transwarp::listener>&) override {}

    /// No-op because a value task doesn't raise events
    void remove_listeners() override {}

    /// No-op because a value task doesn't raise events
    void remove_listeners(transwarp::event_type) override {}

    /// No-op because a value task doesn't raise events
    void remove_listeners_all() override {}

    /// No-op because a value task doesn't raise events
    void remove_listeners_all(transwarp::event_type) override {}

    /// No-op because a value task never runs
    void schedule() override {}

    /// No-op because a value task never runs
    void schedule(transwarp::executor&) override {}

    /// No-op because a value task never runs
    void schedule(bool) override {}

    /// No-op because a value task never runs
    void schedule(transwarp::executor&, bool) override {}

    /// No-op because a value task never runs and doesn't have parents
    void schedule_all() override {}

    /// No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&) override {}

    /// No-op because a value task never runs and doesn't have parents
    void schedule_all(bool) override {}

    /// No-op because a value task never runs and doesn't have parents
    void schedule_all(transwarp::executor&, bool) override {}

    /// Assigns a value to this task
    void set_value(const transwarp::decay_t<result_type>& value) override {
        future_ = transwarp::detail::make_future_with_value<result_type>(value);
    }

    /// Assigns a value to this task
    void set_value(transwarp::decay_t<result_type>&& value) override {
        future_ = transwarp::detail::make_future_with_value<result_type>(std::move(value));
    };

    /// Assigns an exception to this task
    void set_exception(std::exception_ptr exception) override {
        future_ = transwarp::detail::make_future_with_exception<result_type>(exception);
    }

    /// Returns true because a value task is scheduled once on construction
    bool was_scheduled() const noexcept override {
        return true;
    }

    /// No-op because a value task never runs
    void wait() const override {}

    /// Returns true because a value task is always ready
    bool is_ready() const override {
        return true;
    }

    /// Returns true because a value task always contains a result
    bool has_result() const noexcept override {
        return true;
    }

    /// Returns the result of this task
    transwarp::result_t<result_type> get() const override {
        return future_.get();
    }

    /// No-op because a value task never runs
    void reset() override {}

    /// No-op because a value task never runs and doesn't have parents
    void reset_all() override {}

    /// No-op because a value task never runs
    void cancel(bool) noexcept override {}

    /// No-op because a value task never runs and doesn't have parents
    void cancel_all(bool) noexcept override {}

    /// Returns all tasks in the graph in breadth order
    const std::vector<transwarp::itask*>& tasks() override {
        return tasks_;
    }

    /// Returns empty edges because a value task doesn't have parents
    std::vector<transwarp::edge> edges() override {
        return {};
    }

private:

    value_task() = default;

    std::shared_ptr<transwarp::task<result_type>> clone_impl(std::unordered_map<std::shared_ptr<transwarp::itask>, std::shared_ptr<transwarp::itask>>&) const override {
        auto t = std::shared_ptr<value_task>{new value_task{}};
        t->node_ = node_->clone();
        try {
            t->set_value(future_.get());
        } catch (...) {
            t->set_exception(std::current_exception());
        }
        t->visited_ = visited_;
        return t;
    }

    /// Assigns the given id to the node
    void set_node_id(std::size_t id) noexcept override {
        transwarp::detail::node_manip::set_id(*node_, id);
    }

    /// No-op because a value task never runs
    void schedule_impl(bool, transwarp::executor*) override {}

    /// Visits this task
    void visit(const std::function<void(transwarp::itask&)>& visitor) override {
        if (!visited_) {
            visitor(*this);
            visited_ = true;
        }
    }

    /// Marks this task as not visited
    void unvisit() noexcept override {
        visited_ = false;
    }

    std::shared_ptr<transwarp::node> node_;
    std::shared_future<result_type> future_;
    bool visited_ = false;
    std::vector<transwarp::itask*> tasks_{this};
};


/// A factory function to create a new task
template<typename TaskType, typename Functor, typename... Parents>
auto make_task(TaskType, Functor&& functor, std::shared_ptr<Parents>... parents) {
    using task_t = transwarp::task_impl<TaskType, std::decay_t<Functor>, typename Parents::result_type...>;
    return std::shared_ptr<task_t>{new task_t{std::forward<Functor>(functor), std::move(parents)...}};
}


/// A factory function to create a new task with vector parents
template<typename TaskType, typename Functor, typename ParentType>
auto make_task(TaskType, Functor&& functor, std::vector<ParentType> parents) {
    using task_t = transwarp::task_impl<TaskType, std::decay_t<Functor>, std::vector<ParentType>>;
    return std::shared_ptr<task_t>{new task_t{std::forward<Functor>(functor), std::move(parents)}};
}


/// A factory function to create a new value task
template<typename Value>
auto make_value_task(Value&& value) {
    using task_t = transwarp::value_task<transwarp::decay_t<Value>>;
    return std::shared_ptr<task_t>{new task_t{std::forward<Value>(value)}};
}


/// A function similar to std::for_each but returning a transwarp task for
/// deferred, possibly asynchronous execution. This function creates a graph
/// with std::distance(first, last) root nodes
template<typename InputIt, typename UnaryOperation>
auto for_each(InputIt first, InputIt last, UnaryOperation unary_op) {
    const auto distance = std::distance(first, last);
    if (distance <= 0) {
        throw transwarp::invalid_parameter{"first or last"};
    }
    std::vector<std::shared_ptr<transwarp::task<void>>> tasks;
    tasks.reserve(static_cast<std::size_t>(distance));
    for (; first != last; ++first) {
        tasks.push_back(transwarp::make_task(transwarp::root, [unary_op,first]{ unary_op(*first); }));
    }
    auto final = transwarp::make_task(transwarp::wait, transwarp::no_op, tasks);
    final->finalize();
    return final;
}

/// A function similar to std::for_each but returning a transwarp task for
/// deferred, possibly asynchronous execution. This function creates a graph
/// with std::distance(first, last) root nodes.
/// Overload for automatic scheduling by passing an executor.
template<typename InputIt, typename UnaryOperation>
auto for_each(transwarp::executor& executor, InputIt first, InputIt last, UnaryOperation unary_op) {
    auto task = transwarp::for_each(first, last, unary_op);
    task->schedule_all(executor);
    return task;
}


/// A function similar to std::transform but returning a transwarp task for
/// deferred, possibly asynchronous execution. This function creates a graph
/// with std::distance(first1, last1) root nodes
template<typename InputIt, typename OutputIt, typename UnaryOperation>
auto transform(InputIt first1, InputIt last1, OutputIt d_first, UnaryOperation unary_op) {
    const auto distance = std::distance(first1, last1);
    if (distance <= 0) {
        throw transwarp::invalid_parameter{"first1 or last1"};
    }
    std::vector<std::shared_ptr<transwarp::task<void>>> tasks;
    tasks.reserve(static_cast<std::size_t>(distance));
    for (; first1 != last1; ++first1, ++d_first) {
        tasks.push_back(transwarp::make_task(transwarp::root, [unary_op,first1,d_first]{ *d_first = unary_op(*first1); }));
    }
    auto final = transwarp::make_task(transwarp::wait, transwarp::no_op, tasks);
    final->finalize();
    return final;
}

/// A function similar to std::transform but returning a transwarp task for
/// deferred, possibly asynchronous execution. This function creates a graph
/// with std::distance(first1, last1) root nodes.
/// Overload for automatic scheduling by passing an executor.
template<typename InputIt, typename OutputIt, typename UnaryOperation>
auto transform(transwarp::executor& executor, InputIt first1, InputIt last1, OutputIt d_first, UnaryOperation unary_op) {
    auto task = transwarp::transform(first1, last1, d_first, unary_op);
    task->schedule_all(executor);
    return task;
}


/// A task pool that allows running multiple instances of the same task in parallel.
template<typename ResultType>
class task_pool {
public:

    /// Constructs a task pool
    task_pool(std::shared_ptr<transwarp::task<ResultType>> task,
               std::size_t minimum_size,
               std::size_t maximum_size)
    : task_{std::move(task)},
      minimum_{minimum_size},
      maximum_{maximum_size},
      finished_{maximum_size}
    {
        if (minimum_ < 1) {
            throw transwarp::invalid_parameter{"minimum size"};
        }
        if (minimum_ > maximum_) {
            throw transwarp::invalid_parameter{"minimum or maximum size"};
        }
        task_->add_listener(transwarp::event_type::after_finished, listener_);
        for (std::size_t i=0; i<minimum_; ++i) {
            idle_.push(task_->clone());
        }
    }

    /// Constructs a task pool with reasonable defaults for minimum and maximum
    explicit
    task_pool(std::shared_ptr<transwarp::task<ResultType>> task)
    : task_pool{std::move(task), 32, 65536}
    {}

    // delete copy/move semantics
    task_pool(const task_pool&) = delete;
    task_pool& operator=(const task_pool&) = delete;
    task_pool(task_pool&&) = delete;
    task_pool& operator=(task_pool&&) = delete;

    /// Returns the next idle task.
    /// If there are no idle tasks then it will attempt to double the
    /// pool size. If that fails then it will return a nullptr. On successful
    /// retrieval of an idle task the function will mark that task as busy.
    std::shared_ptr<transwarp::task<ResultType>> next_task(bool maybe_resize=true) {
        std::shared_ptr<transwarp::node> finished_node;
        {
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            if (!finished_.empty()) {
                finished_node = finished_.front(); finished_.pop();
            }
        }

        std::shared_ptr<transwarp::task<ResultType>> task;
        if (finished_node) {
            task = busy_.find(finished_node)->second;
        } else {
            if (maybe_resize && idle_.empty()) {
                resize(size() * 2); // double pool size
            }
            if (idle_.empty()) {
                return nullptr;
            }
            task = idle_.front(); idle_.pop();
            busy_.emplace(task->node(), task);
        }

        const auto& future = task->future();
        if (future.valid()) {
            future.wait(); // will return immediately
        }
        return task;
    }

    /// Just like next_task() but waits for a task to become available.
    /// The returned graph will always be a valid pointer
    std::shared_ptr<transwarp::task<ResultType>> wait_for_next_task(bool maybe_resize=true) {
        for (;;) {
            std::shared_ptr<transwarp::task<ResultType>> g = next_task(maybe_resize);
            if (g) {
                return g;
            }
        }
    }

    /// Returns the current total size of the pool (sum of idle and busy tasks)
    std::size_t size() const {
        return idle_.size() + busy_.size();
    }

    /// Returns the minimum size of the pool
    std::size_t minimum_size() const {
        return minimum_;
    }

    /// Returns the maximum size of the pool
    std::size_t maximum_size() const {
        return maximum_;
    }

    /// Returns the number of idle tasks in the pool
    std::size_t idle_count() const {
        std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
        return idle_.size() + finished_.size();
    }

    /// Returns the number of busy tasks in the pool
    std::size_t busy_count() const {
        std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
        return busy_.size() - finished_.size();
    }

    /// Resizes the task pool to the given new size if possible
    void resize(std::size_t new_size) {
        reclaim();
        if (new_size > size()) { // grow
            const std::size_t count = new_size - size();
            for (std::size_t i=0; i<count; ++i) {
                if (size() == maximum_) {
                    break;
                }
                idle_.push(task_->clone());
            }
        } else if (new_size < size()) { // shrink
            const std::size_t count = size() - new_size;
            for (std::size_t i=0; i<count; ++i) {
                if (idle_.empty() || size() == minimum_) {
                    break;
                }
                idle_.pop();
            }
        }
    }

    /// Reclaims finished tasks by marking them as idle again
    void reclaim() {
        decltype(finished_) finished{finished_.capacity()};
        {
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            finished_.swap(finished);
        }
        while (!finished.empty()) {
            const std::shared_ptr<transwarp::node> node = finished.front(); finished.pop();
            const auto it = busy_.find(node);
            idle_.push(it->second);
            busy_.erase(it);
        }
    }

private:

    class finished_listener : public transwarp::listener {
    public:

        explicit
        finished_listener(task_pool<ResultType>& pool)
        : pool_{pool}
        {}

        // Called on a potentially high-priority thread
        void handle_event(transwarp::event_type, const transwarp::itask& task) override {
            std::lock_guard<transwarp::detail::spinlock> lock{pool_.spinlock_};
            pool_.finished_.push(task.node());
        }

    private:
        task_pool<ResultType>& pool_;
    };

    std::shared_ptr<transwarp::task<ResultType>> task_;
    std::size_t minimum_;
    std::size_t maximum_;
    mutable transwarp::detail::spinlock spinlock_; // protecting finished_
    transwarp::detail::circular_buffer<std::shared_ptr<transwarp::node>> finished_;
    std::queue<std::shared_ptr<transwarp::task<ResultType>>> idle_;
    std::unordered_map<std::shared_ptr<transwarp::node>, std::shared_ptr<transwarp::task<ResultType>>> busy_;
    std::shared_ptr<transwarp::listener> listener_{new finished_listener{*this}};
};


/// A timer that tracks the average idle, wait, and run time of each task it listens to.
/// - idle = time between scheduling and starting the task (executor dependent)
/// - wait = time between starting and invoking the task's functor, i.e. wait for parent tasks to finish
/// - run = time between invoking and finishing the task's computations
class timer : public transwarp::listener {
public:
    timer() = default;

    // delete copy/move semantics
    timer(const timer&) = delete;
    timer& operator=(const timer&) = delete;
    timer(timer&&) = delete;
    timer& operator=(timer&&) = delete;

    /// Performs the actual timing and populates the node's timing members
    void handle_event(transwarp::event_type event, const transwarp::itask& task) override {
        switch (event) {
        case transwarp::event_type::before_scheduled: {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[task.node()];
            track.startidle = now;
        }
        break;
        case transwarp::event_type::before_started: {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            track_idletime(task.node(), now);
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[task.node()];
            track.startwait = now;
        }
        break;
        case transwarp::event_type::after_canceled: {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            track_waittime(task.node(), now);
        }
        break;
        case transwarp::event_type::before_invoked: {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            track_waittime(task.node(), now);
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[task.node()];
            track.running = true;
            track.startrun = now;
        }
        break;
        case transwarp::event_type::after_finished: {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            track_runtime(task.node(), now);
        }
        break;
        default: break;
        }
    }

    /// Resets all timing information
    void reset() {
        std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
        tracks_.clear();
    }

private:

    void track_idletime(const std::shared_ptr<transwarp::node>& node, const std::chrono::time_point<std::chrono::steady_clock>& now) {
        std::int64_t avg_idletime_us;
        {
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[node];
            track.idletime += std::chrono::duration_cast<std::chrono::microseconds>(now - track.startidle).count();
            ++track.idlecount;
            avg_idletime_us = static_cast<std::int64_t>(track.idletime / track.idlecount);
        }
        transwarp::detail::node_manip::set_avg_idletime_us(*node, avg_idletime_us);
    };

    void track_waittime(const std::shared_ptr<transwarp::node>& node, const std::chrono::time_point<std::chrono::steady_clock>& now) {
        std::int64_t avg_waittime_us;
        {
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[node];
            track.waittime += std::chrono::duration_cast<std::chrono::microseconds>(now - track.startwait).count();
            ++track.waitcount;
            avg_waittime_us = static_cast<std::int64_t>(track.waittime / track.waitcount);
        }
        transwarp::detail::node_manip::set_avg_waittime_us(*node, avg_waittime_us);
    };

    void track_runtime(const std::shared_ptr<transwarp::node>& node, const std::chrono::time_point<std::chrono::steady_clock>& now) {
        std::int64_t avg_runtime_us;
        {
            std::lock_guard<transwarp::detail::spinlock> lock{spinlock_};
            auto& track = tracks_[node];
            if (!track.running) {
                return;
            }
            track.running = false;
            track.runtime += std::chrono::duration_cast<std::chrono::microseconds>(now - track.startrun).count();
            ++track.runcount;
            avg_runtime_us = static_cast<std::int64_t>(track.runtime / track.runcount);
        }
        transwarp::detail::node_manip::set_avg_runtime_us(*node, avg_runtime_us);
    }

    struct track {
        bool running = false;
        std::chrono::time_point<std::chrono::steady_clock> startidle;
        std::chrono::time_point<std::chrono::steady_clock> startwait;
        std::chrono::time_point<std::chrono::steady_clock> startrun;
        std::chrono::microseconds::rep idletime = 0;
        std::chrono::microseconds::rep idlecount = 0;
        std::chrono::microseconds::rep waittime = 0;
        std::chrono::microseconds::rep waitcount = 0;
        std::chrono::microseconds::rep runtime = 0;
        std::chrono::microseconds::rep runcount = 0;
    };

    transwarp::detail::spinlock spinlock_; // protecting tracks_
    std::unordered_map<std::shared_ptr<transwarp::node>, track> tracks_;
};


} // transwarp
