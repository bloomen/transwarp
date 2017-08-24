// transwarp is a header-only C++ library for task concurrency
// Version: in dev
// Repository: https://github.com/bloomen/transwarp
// Copyright: 2017 Christian Blume
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
    if (type == transwarp::task_type::root) {
        return "root";
    } else if (type == transwarp::task_type::consume) {
        return "consume";
    } else if (type == transwarp::task_type::consume_any) {
        return "consume_any";
    } else if (type == transwarp::task_type::wait) {
        return "wait";
    } else if (type == transwarp::task_type::wait_any) {
        return "wait_any";
    }
    return "unknown";
}


// The root type. Used for tag dispatch
struct root_type : std::integral_constant<transwarp::task_type, transwarp::task_type::root> {};
constexpr const transwarp::root_type root{};

// The consume type. Used for tag dispatch
struct consume_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume> {};
constexpr const transwarp::consume_type consume{};

// The consume_any type. Used for tag dispatch
struct consume_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::consume_any> {};
constexpr const transwarp::consume_any_type consume_any{};

// The wait type. Used for tag dispatch
struct wait_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait> {};
constexpr const transwarp::wait_type wait{};

// The wait_any type. Used for tag dispatch
struct wait_any_type : std::integral_constant<transwarp::task_type, transwarp::task_type::wait_any> {};
constexpr const transwarp::wait_any_type wait_any{};


namespace detail {

// Trims the given characters from the input string
inline std::string trim(const std::string &s, const std::string& chars=" \t\n\r") {
    auto functor = [&chars](char c) { return chars.find(c) != std::string::npos; };
    auto it = std::find_if_not(s.begin(), s.end(), functor);
    return std::string(it, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(it), functor).base());
}

} // detail


// A node carrying meta-data of a task
struct node {
    std::size_t id;
    std::string name;
    transwarp::task_type type;
    std::vector<std::shared_ptr<node>> parents;
};

// String conversion for the node class
inline std::string to_string(const transwarp::node& node) {
    const auto name = transwarp::detail::trim(node.name);
    std::string s;
    s += '"';
    s += name + "\n";
    s += transwarp::to_string(node.type) + "\n";
    s += "id " + std::to_string(node.id);
    s += " parents " + std::to_string(node.parents.size());
    s += '"';
    return s;
}


// An edge between two nodes
struct edge {
    std::shared_ptr<transwarp::node> parent;
    std::shared_ptr<transwarp::node> child;
};

// String conversion for the edge class
inline std::string to_string(const transwarp::edge& edge) {
    return transwarp::to_string(*edge.parent) + " -> " + transwarp::to_string(*edge.child);
}


// Creates a dot-style string from the given graph
inline std::string to_string(const std::vector<transwarp::edge>& graph) {
    std::string dot = "digraph {\n";
    for (const auto& edge : graph) {
        dot += transwarp::to_string(edge) + '\n';
    }
    dot += "}\n";
    return dot;
}


// The executor interface
class executor {
public:
    virtual ~executor() = default;
    virtual void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>& node) = 0;
};


// An interface for the task class
template<typename ResultType>
class itask {
public:
    virtual ~itask() = default;
    virtual void set_executor(std::shared_ptr<transwarp::executor> executor) = 0;
    virtual std::shared_future<ResultType> get_future() const = 0;
    virtual const std::shared_ptr<transwarp::node>& get_node() const = 0;
    virtual void schedule() = 0;
    virtual void schedule(transwarp::executor& executor) = 0;
    virtual void schedule_all() = 0;
    virtual void schedule_all(transwarp::executor& executor) = 0;
    virtual void reset() = 0;
    virtual void reset_all() = 0;
    virtual void cancel(bool enabled) = 0;
    virtual void cancel_all(bool enabled) = 0;
    virtual std::vector<transwarp::edge> get_graph() const = 0;
};


// Base class for exceptions
class transwarp_error : public std::runtime_error {
public:
    explicit transwarp_error(const std::string& message)
    : std::runtime_error(message) {}
};


// Exception thrown when a task is canceled
class task_canceled : public transwarp::transwarp_error {
public:
    explicit task_canceled(const transwarp::node& n)
    : transwarp::transwarp_error(n.name + " is canceled") {}
};


namespace detail {


// An exception for errors in the thread_pool class
class thread_pool_error : public transwarp::transwarp_error {
public:
    explicit thread_pool_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};

// A simple thread pool used to execute tasks in parallel
class thread_pool {
public:

    explicit thread_pool(std::size_t n_threads)
    : done_(false)
    {
        if (n_threads > 0) {
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
        } else {
            throw transwarp::detail::thread_pool_error("number of threads must be larger than zero");
        }
    }

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
    }

    bool done_;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> functors_;
    std::condition_variable cond_var_;
    std::mutex mutex_;
};

template<typename TaskType, bool done, int total, int... n>
struct call_with_futures_impl {
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
        return call_with_futures_impl<TaskType, total == 1 + sizeof...(n), total, n..., sizeof...(n)>::template
                work<Result>(canceled, node, f, t);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::root_type, true, total, n...> {
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple&) {
        if (canceled) {
            throw transwarp::task_canceled(node);
        }
        return f();
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_type, true, total, n...> {
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
        auto results = std::tie(std::get<n>(t).get()...);
        if (canceled) {
            throw transwarp::task_canceled(node);
        }
        return f(std::get<n>(results)...);
    }
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::consume_any_type, true, total, n...> {
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
        using future_t = typename std::remove_reference<decltype(std::get<0>(t))>::type; // use first type as reference
        for (;;) {
            bool ready = false;
            auto future = waiter<future_t>::template wait(ready, std::get<n>(t)...);
            if (ready) {
                if (canceled) {
                    throw transwarp::task_canceled(node);
                }
                return f(future.get());
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
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
        wait(std::get<n>(t)...);
        if (canceled) {
            throw transwarp::task_canceled(node);
        }
        return f();
    }
    template<typename T, typename... Args>
    static void wait(const T& arg, const Args& ...args) {
        arg.wait();
        wait(args...);
    }
    static void wait() {}
};

template<int total, int... n>
struct call_with_futures_impl<transwarp::wait_any_type, true, total, n...> {
    template<typename Result, typename Functor, typename Tuple>
    static Result work(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
        while (!wait(std::get<n>(t)...));
        if (canceled) {
            throw transwarp::task_canceled(node);
        }
        return f();
    }
    template<typename T, typename... Args>
    static bool wait(const T& arg, const Args& ...args) {
        const auto status = arg.wait_for(std::chrono::microseconds(1));
        if (status == std::future_status::ready) {
            return true;
        }
        return wait(args...);
    }
    static bool wait() {
        return false;
    }
};

// Calls the given functor with or without the tuple of futures depending on the task type.
// Throws task_canceled if the canceled becomes true
template<typename TaskType, typename Result, typename Functor, typename Tuple>
Result call_with_futures(const std::atomic_bool& canceled, const transwarp::node& node, const Functor& f, const Tuple& t) {
    using tuple_t = typename std::decay<Tuple>::type;
    static const std::size_t n = std::tuple_size<tuple_t>::value;
    return transwarp::detail::call_with_futures_impl<TaskType, 0 == n, n>::template
            work<Result>(canceled, node, f, t);
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
    using tuple_t = typename std::decay<Tuple>::type;
    static const std::size_t n = std::tuple_size<tuple_t>::value;
    using index_t = typename transwarp::detail::index_range<0, n>::type;
    transwarp::detail::call_with_each_index(index_t(), f, t);
}

template<int offset, typename... Tasks>
struct assign_futures_impl {
    static void work(const std::tuple<std::shared_ptr<Tasks>...>& source, std::tuple<std::shared_future<typename Tasks::result_type>...>& target) {
        std::get<offset>(target) = std::get<offset>(source)->get_future();
        assign_futures_impl<offset - 1, Tasks...>::work(source, target);
    }
};

template<typename... Tasks>
struct assign_futures_impl<-1, Tasks...> {
    static void work(const std::tuple<std::shared_ptr<Tasks>...>&, std::tuple<std::shared_future<typename Tasks::result_type>...>&) {}
};

// Returns the futures from the given tasks
template<typename... Tasks>
std::tuple<std::shared_future<typename Tasks::result_type>...> get_futures(const std::tuple<std::shared_ptr<Tasks>...>& input) {
    std::tuple<std::shared_future<typename Tasks::result_type>...> result;
    assign_futures_impl<static_cast<int>(sizeof...(Tasks)) - 1, Tasks...>::work(input, result);
    return result;
}

// Sets parents of the node
struct parent_visitor {
    explicit parent_visitor(std::shared_ptr<transwarp::node>& node) noexcept
    : node_(node) {}

    template<typename Task>
    void operator()(const Task& task) const {
        node_->parents.push_back(task.node_);
    }

    std::shared_ptr<transwarp::node>& node_;
};

// Collects edges from the given node and task objects
struct edges_visitor {
    edges_visitor(std::vector<transwarp::edge>& graph, const std::shared_ptr<transwarp::node>& node) noexcept
    : graph_(graph), node_(node) {}

    template<typename Task>
    void operator()(const Task& task) const {
        graph_.push_back({task.node_, node_});
    }

    std::vector<transwarp::edge>& graph_;
    const std::shared_ptr<transwarp::node>& node_;
};

// Applies final bookkeeping to the task
struct final_visitor {
    final_visitor() noexcept
    : id_(0) {}

    template<typename Task>
    void operator()(Task& task) {
        task.node_->id = id_++;
        if (task.node_->name.empty())
            task.node_->name = "task";
    }

    std::size_t id_;
    std::shared_ptr<std::atomic_bool> canceled_;
};

// Generates a graph
struct graph_visitor {
    explicit graph_visitor(std::vector<transwarp::edge>& graph) noexcept
    : graph_(graph) {}

    template<typename Task>
    void operator()(const Task& task) {
        transwarp::detail::call_with_each(transwarp::detail::edges_visitor(graph_, task.node_), task.parents_);
    }

    std::vector<transwarp::edge>& graph_;
};

// Schedules using the given executor
struct schedule_visitor {
    explicit schedule_visitor(transwarp::executor* executor) noexcept
    : executor_(executor) {}

    template<typename Task>
    void operator()(Task& task) {
        task.schedule_impl(executor_);
    }

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

    template<typename Task>
    void operator()(Task& task) const {
        task.cancel(enabled_);
    }

    bool enabled_;
};

// Visits the given task using the visitors given in the constructor
template<typename Visitor>
struct visit {
    explicit visit(Visitor& visitor) noexcept
    : visitor_(visitor) {}

    template<typename Task>
    void operator()(Task& task) const {
        task.visit(visitor_);
    }

    Visitor& visitor_;
};

// Unvisits the given task
struct unvisit {

    template<typename Task>
    void operator()(Task& task) const noexcept {
        task.unvisit();
    }
};

// Determines the result type of the Functor dispatching on the task type
template<typename TaskType, typename Functor, typename... Tasks>
struct result {
    static_assert(std::is_same<TaskType, transwarp::root_type>::value ||
                  std::is_same<TaskType, transwarp::consume_type>::value ||
                  std::is_same<TaskType, transwarp::consume_any_type>::value ||
                  std::is_same<TaskType, transwarp::wait_type>::value ||
                  std::is_same<TaskType, transwarp::wait_any_type>::value,
                  "Invalid task type, must be one of: root, consume, consume_any, wait, wait_any");
};

template<typename Functor, typename... Tasks>
struct result<transwarp::root_type, Functor, Tasks...> {
    static_assert(sizeof...(Tasks) == 0, "A root task cannot have parent tasks");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... Tasks>
struct result<transwarp::consume_type, Functor, Tasks...> {
    static_assert(sizeof...(Tasks) > 0, "A consume task must have at least one parent");
    using type = decltype(std::declval<Functor>()(std::declval<typename Tasks::result_type>()...));
};

template<typename Functor, typename... Tasks>
struct result<transwarp::consume_any_type, Functor, Tasks...> {
    static_assert(sizeof...(Tasks) > 0, "A consume_any task must have at least one parent");
    using arg_t = typename std::tuple_element<0, std::tuple<typename Tasks::result_type...>>::type; // using first type as reference
    using type = decltype(std::declval<Functor>()(std::declval<arg_t>()));
};

template<typename Functor, typename... Tasks>
struct result<transwarp::wait_type, Functor, Tasks...> {
    static_assert(sizeof...(Tasks) > 0, "A wait task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

template<typename Functor, typename... Tasks>
struct result<transwarp::wait_any_type, Functor, Tasks...> {
    static_assert(sizeof...(Tasks) > 0, "A wait_any task must have at least one parent");
    using type = decltype(std::declval<Functor>()());
};

} // detail


// Executor for sequential execution. Runs functors sequentially on the same thread
class sequential : public transwarp::executor {
public:

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

    void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>&) override {
        pool_.push(functor);
    }

private:
    transwarp::detail::thread_pool pool_;
};


// A task representing a piece work given by a functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
template<typename TaskType, typename Functor, typename... Tasks>
class task : public transwarp::itask<typename transwarp::detail::result<TaskType, Functor, Tasks...>::type> {
public:
    // The task type
    using task_type = TaskType;

    // The result type of this task
    using result_type = typename transwarp::detail::result<task_type, Functor, Tasks...>::type;

    // A task is defined by name, functor, and parent tasks
    // name is optional. See constructor overload
    // cppcheck-suppress passedByValue
    task(std::string name, Functor functor, std::shared_ptr<Tasks>... parents)
    : node_(std::make_shared<transwarp::node>(transwarp::node{0, std::move(name), task_type::value, {}})),
      functor_(std::move(functor)),
      parents_(std::make_tuple(std::move(parents)...)),
      visited_(false),
      canceled_(false)
    {
        transwarp::detail::call_with_each(transwarp::detail::parent_visitor(node_), parents_);
        transwarp::detail::final_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // This overload is for auto-naming
    // cppcheck-suppress uninitMemberVar
    explicit task(Functor functor, std::shared_ptr<Tasks>... parents)
    : task("", std::move(functor), std::move(parents)...)
    {}

    virtual ~task() = default;

    // delete copy/move semantics
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&&) = delete;
    task& operator=(task&&) = delete;

    // Assigns an executor to this task which takes precedence over
    // the executor provided in schedule() or schedule_all()
    void set_executor(std::shared_ptr<transwarp::executor> executor) override {
        if (!executor) {
            throw transwarp::transwarp_error("Not a valid pointer to executor");
        }
        executor_ = std::move(executor);
    }

    // Returns the future associated to the underlying execution
    std::shared_future<result_type> get_future() const override {
        return future_;
    }

    // Returns the associated node
    const std::shared_ptr<transwarp::node>& get_node() const override {
        return node_;
    }

    // Schedules this task for execution on the caller thread.
    // The task-specific executor gets precedence if it exists.
    void schedule() override {
        schedule_impl();
    }

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    void schedule(transwarp::executor& executor) override {
        schedule_impl(&executor);
    }

    // Schedules all tasks in the graph for execution on the caller thread.
    // The task-specific executors get precedence if they exist.
    void schedule_all() override {
        schedule_all_impl();
    }

    // Schedules all tasks in the graph for execution using the provided executor.
    // The task-specific executors get precedence if they exist.
    void schedule_all(transwarp::executor& executor) override {
        schedule_all_impl(&executor);
    }

    // Resets the future of this task, allowing for a new call to schedule
    void reset() override {
        future_ = std::shared_future<result_type>();
    }

    // Resets the futures of all tasks in the graph, allowing for re-schedule
    // of all tasks in the graph
    void reset_all() override {
        transwarp::detail::reset_visitor visitor;
        visit(visitor);
        unvisit();
    }

    // If enabled then this task is canceled which will
    // throw transwarp::task_canceled when asking the future for its result.
    // Canceling pending tasks does not affect currently running tasks.
    // As long as cancel is enabled new computations cannot be scheduled.
    // Passing true is equivalent to resume.
    void cancel(bool enabled) override {
        canceled_ = enabled;
    }

    // If enabled then all pending tasks in the graph are canceled which will
    // throw transwarp::task_canceled when asking a future for its result.
    // Canceling pending tasks does not affect currently running tasks.
    // As long as cancel is enabled new computations cannot be scheduled.
    // Passing true is equivalent to resume.
    void cancel_all(bool enabled) override {
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
        const_cast<task*>(this)->visit(visitor);
        const_cast<task*>(this)->unvisit();
        return graph;
    }

private:

    friend struct transwarp::detail::parent_visitor;
    friend struct transwarp::detail::edges_visitor;
    friend struct transwarp::detail::graph_visitor;
    friend struct transwarp::detail::final_visitor;
    friend struct transwarp::detail::schedule_visitor;

    template<typename T>
    friend struct transwarp::detail::visit;
    friend struct transwarp::detail::unvisit;

    // Schedules this task for execution using the provided executor.
    // The task-specific executor gets precedence if it exists.
    // Runs the task on the same thread as the caller if neither the global
    // nor the task-specific executor is found.
    void schedule_impl(transwarp::executor* executor=nullptr) {
        if (!canceled_ && !future_.valid()) {
            auto futures = transwarp::detail::get_futures(parents_);
            auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
                    std::bind(&task::evaluate, std::ref(*this), std::move(futures)));
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
    void schedule_all_impl(transwarp::executor* executor=nullptr) {
        if (!canceled_) {
            transwarp::detail::schedule_visitor visitor(executor);
            visit(visitor);
            unvisit();
        }
    }

    // Visits each task in a depth-first traversal. The visitor
    // takes a reference to a task (task&) as its only input argument.
    template<typename Visitor>
    void visit(Visitor& visitor) {
        if (!visited_) {
            transwarp::detail::call_with_each(transwarp::detail::visit<Visitor>(visitor), parents_);
            visitor(*this);
            visited_ = true;
        }
    }

    // Traverses through all tasks and marks them as not visited.
    void unvisit() noexcept {
        if (visited_) {
            visited_ = false;
            transwarp::detail::call_with_each(transwarp::detail::unvisit(), parents_);
        }
    }

    // Calls the functor of the given task with the results from the futures.
    // Throws transwarp::task_canceled if the task is canceled.
    static result_type evaluate(transwarp::task<task_type, Functor, Tasks...>& task,
                                std::tuple<std::shared_future<typename Tasks::result_type>...> futures) {
        return transwarp::detail::call_with_futures<task_type, result_type>(task.canceled_, *task.node_, task.functor_, futures);
    }

    std::shared_ptr<transwarp::node> node_;
    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> parents_;
    bool visited_;
    std::atomic_bool canceled_;
    std::shared_ptr<transwarp::executor> executor_;
    std::shared_future<result_type> future_;
};


// A factory function to create a new task
template<typename TaskType, typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<TaskType, Functor, Tasks...>> make_task(TaskType, std::string name, Functor functor, std::shared_ptr<Tasks>... parents) {
    return std::make_shared<transwarp::task<TaskType, Functor, Tasks...>>(std::move(name), std::move(functor), std::move(parents)...);
}

// A factory function to create a new task. Overload for auto-naming
template<typename TaskType, typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<TaskType, Functor, Tasks...>> make_task(TaskType, Functor functor, std::shared_ptr<Tasks>... parents) {
    return std::make_shared<transwarp::task<TaskType, Functor, Tasks...>>(std::move(functor), std::move(parents)...);
}


} // transwarp
