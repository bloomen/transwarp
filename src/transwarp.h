// transwarp is a header-only C++ library for task concurrency
// Version: in development
// Author: Christian Blume (chr.blume@gmail.com)
// Repository: https://github.com/bloomen/transwarp
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

// A node carrying meta-data of a task
struct node {
    std::size_t id;
    std::size_t level;
    std::string name;
    std::vector<const node*> parents;
};

// An edge between two nodes
struct edge {
    const transwarp::node* child;
    const transwarp::node* parent;
};

// An interface for the task class
template<typename ResultType>
class itask {
public:
    virtual ~itask() = default;
    virtual std::shared_future<ResultType> get_future() const = 0;
    virtual const transwarp::node& get_node() const = 0;
};

// An interface for the final_task class
template<typename ResultType>
class ifinal_task {
public:
    virtual ~ifinal_task() = default;
    virtual std::shared_future<ResultType> get_future() const = 0;
    virtual const transwarp::node& get_node() const = 0;
    virtual void set_parallel(std::size_t n_threads) = 0;
    virtual void schedule() = 0;
    virtual void set_pause(bool enabled) = 0;
    virtual void set_cancel(bool enabled) = 0;
    virtual const std::vector<transwarp::edge>& get_graph() const = 0;
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
    task_canceled(const transwarp::node& n)
    : transwarp::transwarp_error(n.name + " is canceled") {}
};


namespace detail {

template<typename T, typename... Args>
std::unique_ptr<T> make_unique_helper(std::false_type, Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T, typename... Args>
std::unique_ptr<T> make_unique_helper(std::true_type, Args&&... args) {
    static_assert(std::extent<T>::value == 0, "use make_unique<T[]>().");
    using U = typename std::remove_extent<T>::type;
    return std::unique_ptr<T>(new U[sizeof...(Args)]{std::forward<Args>(args)...});
}

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return transwarp::detail::make_unique_helper<T>(std::is_array<T>(), std::forward<Args>(args)...);
}

class priority_functor {
public:

    priority_functor() noexcept
    : callback_{}, node_{0, 0, "", {}} {}

    priority_functor(std::function<std::function<void()>()> callback, transwarp::node node) noexcept
    : callback_(std::move(callback)), node_(std::move(node)) {}

    bool operator>(const priority_functor& other) const noexcept {
        return std::tie(node_.level, node_.id) > std::tie(other.node_.level, other.node_.id);
    }

    std::function<void()> operator()() const {
        return callback_();
    }

private:
    std::function<std::function<void()>()> callback_;
    transwarp::node node_;
};

class thread_pool_error : public transwarp::transwarp_error {
public:
    explicit thread_pool_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};

class thread_pool {
public:

    explicit thread_pool(std::size_t n_threads)
    : done_{false}, paused_{false}
    {
        if (n_threads > 0) {
            const auto n_target = threads_.size() + n_threads;
            while (threads_.size() < n_target) {
                threads_.emplace_back(&thread_pool::worker, this);
            }
        } else {
            throw transwarp::detail::thread_pool_error{"number of threads must be larger than zero"};
        }
    }

    ~thread_pool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
            paused_ = false;
        }
        cond_var_.notify_all();
        for (auto& thread : threads_)
            thread.join();
    }

    void push(const std::function<void()>& functor) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (done_)
                throw transwarp::detail::thread_pool_error{"push called while thread pool is shutting down"};
            functors_.push(functor);
        }
        cond_var_.notify_one();
    }

    void set_pause(bool enabled) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            paused_ = enabled;
        }
        if (!paused_)
            cond_var_.notify_all();
    }

private:

    void worker() {
        for (;;) {
            std::function<void()> functor;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_var_.wait(lock, [this]{
                    return !paused_ && (done_ || !functors_.empty());
                });
                if (done_ && functors_.empty())
                    break;
                functor = functors_.front();
                functors_.pop();
            }
            functor();
        }
    }

    bool done_;
    bool paused_;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> functors_;
    std::condition_variable cond_var_;
    std::mutex mutex_;
};

template<bool Done, int Total, int... N>
struct call_impl {
    template<typename Result, typename F, typename Tuple>
    static Result call(F&& f, Tuple&& t) {
        return call_impl<Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::template
                call<Result>(std::forward<F>(f), std::forward<Tuple>(t));
    }
};

template<int Total, int... N>
struct call_impl<true, Total, N...> {
    template<typename Result, typename F, typename Tuple>
    static Result call(F&& f, Tuple&& t) {
        return std::forward<F>(f)(std::get<N>(std::forward<Tuple>(t)).get()...);
    }
};

template<typename Result, typename F, typename Tuple>
Result call(F&& f, Tuple&& t) {
    using ttype = typename std::decay<Tuple>::type;
    return transwarp::detail::call_impl<0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::template
            call<Result>(std::forward<F>(f), std::forward<Tuple>(t));
}

template<std::size_t ...> struct indices {};

template<std::size_t ...> struct construct_range;

template<std::size_t end, std::size_t idx, std::size_t ...i >
struct construct_range<end, idx, i...> : construct_range<end, idx+1, i..., idx> {};

template<std::size_t end, std::size_t ...i >
struct construct_range< end, end, i... > {
    using type = transwarp::detail::indices< i... >;
};

template<std::size_t b, std::size_t e>
struct index_range {
    using type = typename transwarp::detail::construct_range<e, b>::type;
};

template<typename F, typename Tuple, typename ...Args>
void tuple_for_each_index(transwarp::detail::indices<>, F&&, Tuple&&, Args&&...) {}

template<std::size_t i, std::size_t ...j, typename F, typename Tuple, typename ...Args>
void tuple_for_each_index(transwarp::detail::indices<i,j...>, F&& f, Tuple&& t, Args&&... args) {
    std::forward<F>(f)(*std::get<i>(std::forward<Tuple>(t)), std::forward<Args>(args)...);
    transwarp::detail::tuple_for_each_index(transwarp::detail::indices<j...>(),
            std::forward<F>(f), std::forward<Tuple>(t), std::forward<Args>(args)...);
}

template<typename F, typename Tuple, typename ...Args>
void apply(F&& f, Tuple&& t, Args&&... args) {
    using ttype = typename std::decay<Tuple>::type;
    static const std::size_t n = std::tuple_size<ttype>::value;
    using index_list = typename transwarp::detail::index_range<0, n>::type;
    transwarp::detail::tuple_for_each_index(index_list(),
            std::forward<F>(f), std::forward<Tuple>(t), std::forward<Args>(args)...);
}

template<int offset, typename... Tasks>
struct assign_futures_impl {
    static void assign_futures(const std::tuple<std::shared_ptr<Tasks>...>& source, std::tuple<std::shared_future<typename Tasks::result_type>...>& target) {
        std::get<offset>(target) = std::get<offset>(source)->get_future();
        assign_futures_impl<offset - 1, Tasks...>::assign_futures(source, target);
    }
};

template<typename... Tasks>
struct assign_futures_impl<-1, Tasks...> {
    static void assign_futures(const std::tuple<std::shared_ptr<Tasks>...>&, std::tuple<std::shared_future<typename Tasks::result_type>...>&) {}
};

template<typename... Tasks>
std::tuple<std::shared_future<typename Tasks::result_type>...> get_futures(const std::tuple<std::shared_ptr<Tasks>...>& input) {
    std::tuple<std::shared_future<typename Tasks::result_type>...> result;
    assign_futures_impl<static_cast<int>(sizeof...(Tasks)) - 1, Tasks...>::assign_futures(input, result);
    return result;
}

inline std::string trim(const std::string &s, const std::string& chars=" \t\n\r") {
    auto functor = [&chars](char c) { return chars.find(c) != std::string::npos; };
    auto it = std::find_if_not(s.begin(), s.end(), functor);
    return std::string(it, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(it), functor).base());
}

struct parent_functor {
    explicit parent_functor(transwarp::node& node)
    : node_(node) {}
    template<typename Task>
    void operator()(const Task& task) const noexcept {
        static_assert(!std::is_base_of<transwarp::ifinal_task<typename Task::result_type>, Task>::value,
                      "input task cannot be a final task");
        if (node_.level < task.node_.level)
            node_.level = task.node_.level;
        node_.parents.push_back(&task.node_);
    }
    transwarp::node& node_;
};

struct edges_functor {
    edges_functor(std::vector<transwarp::edge>& graph, const transwarp::node& n) noexcept
    : graph_(graph), n_(n) {}
    template<typename Task>
    void operator()(const Task& task) const noexcept {
        graph_.push_back({&n_, &task.node_});
    }
    std::vector<transwarp::edge>& graph_;
    const transwarp::node& n_;
};

template<typename PreVisitor, typename PostVisitor>
struct visit_functor {
    visit_functor(PreVisitor& pre_visitor, PostVisitor& post_visitor) noexcept
    : pre_visitor_(pre_visitor), post_visitor_(post_visitor) {}
    template<typename Task>
    void operator()(Task& task) const {
        task.visit(pre_visitor_, post_visitor_);
    }
    PreVisitor& pre_visitor_;
    PostVisitor& post_visitor_;
};

struct unvisit_functor {
    unvisit_functor() noexcept = default;
    template<typename Task>
    void operator()(Task& task) const noexcept {
        task.unvisit();
    }
};

struct final_visitor {
    final_visitor(std::size_t& id, std::vector<transwarp::detail::priority_functor>& packagers,
                  const std::shared_ptr<std::atomic_bool>& canceled, std::vector<transwarp::edge>& graph) noexcept
    : id_(id), packagers_(packagers), canceled_(canceled), graph_(graph) {}
    template<typename Task>
    void operator()(Task& task) const {
        task.node_.id = id_++;
        if (task.node_.name.empty())
            task.node_.name = "task" + std::to_string(task.node_.id);
        packagers_.push_back(task.packager_);
        task.canceled_ = canceled_;
        transwarp::detail::apply(transwarp::detail::edges_functor(graph_, task.node_), task.parents_);
    }
    std::size_t& id_;
    std::vector<transwarp::detail::priority_functor>& packagers_;
    const std::shared_ptr<std::atomic_bool>& canceled_;
    std::vector<transwarp::edge>& graph_;
};

} // detail


// A visitor to be used to do nothing
struct pass_visitor {
    pass_visitor() noexcept = default;
    template<typename Task>
    void operator()(const Task&) const noexcept {}
};

// Creates a dot-style string from the given graph
inline std::string make_dot(const std::vector<transwarp::edge>& graph) {
    auto info = [](const transwarp::node& n) {
        const auto name = transwarp::detail::trim(n.name);
        return '"' + name + "\nid " + std::to_string(n.id) + " level " + std::to_string(n.level)
                   + " parents " + std::to_string(n.parents.size()) + '"';
    };
    std::string dot = "digraph {\n";
    for (const auto& pair : graph) {
        dot += info(*pair.parent) + " -> " + info(*pair.child) + '\n';
    }
    dot += "}\n";
    return dot;
}

// A task representing a piece work given by a functor and parent tasks.
// By connecting tasks a directed acyclic graph is built.
// Note that this class is currently not thread-safe, i.e., all methods
// should be called from the same thread.
template<typename Functor, typename... Tasks>
class task : public transwarp::itask<typename std::result_of<Functor(typename Tasks::result_type...)>::type> {
public:
    // This is the result type of this task.
    // Getting a compiler error here means that the result types of the parent tasks
    // do not match or cannot be converted into the functor's parameters of this task
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    // A task is defined by its name (can be empty), a function object, and
    // an arbitrary number of parent tasks
    task(std::string name, Functor functor, std::shared_ptr<Tasks>... parents)
    : node_{0, 0, std::move(name), {}},
      functor_(std::move(functor)),
      parents_(std::make_tuple(std::move(parents)...)),
      visited_(false),
      packager_([this] {
        auto futures = transwarp::detail::get_futures(parents_);
        auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
                std::bind(&task::evaluate, std::ref(*this), std::move(futures)));
        future_ = pack_task->get_future();
        return [pack_task] { (*pack_task)(); };
      }, node_)
    {
        transwarp::detail::apply(transwarp::detail::parent_functor(node_), parents_);
        if (sizeof...(Tasks) > 0)
            ++node_.level;
    }

    virtual ~task() = default;

    // delete copy/move semantics
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&&) = delete;
    task& operator=(task&&) = delete;

    // Returns the future associated to the underlying execution
    std::shared_future<result_type> get_future() const override {
        return future_;
    }

    // Returns the associated node
    const transwarp::node& get_node() const override {
        return node_;
    }

    // Visits each task in a depth-first traversal. The pre_visitor is called
    // before traversing through parents and the post_visitor after. A visitor
    // takes a reference to a task (task&) as its only input argument.
    template<typename PreVisitor, typename PostVisitor>
    void visit(PreVisitor& pre_visitor, PostVisitor& post_visitor) {
        if (!visited_) {
            pre_visitor(*this);
            transwarp::detail::apply(transwarp::detail::visit_functor<PreVisitor, PostVisitor>(pre_visitor, post_visitor), parents_);
            post_visitor(*this);
            visited_ = true;
        }
    }

    // Traverses through all tasks and marks them as not visited.
    void unvisit() {
        if (visited_) {
            visited_ = false;
            transwarp::detail::apply(transwarp::detail::unvisit_functor(), parents_);
        }
    }

    // Returns the functor
    const Functor& get_functor() const {
        return functor_;
    }

    // Returns the parent tasks
    const std::tuple<std::shared_ptr<Tasks>...>& get_parents() const {
        return parents_;
    }

protected:

    friend struct transwarp::detail::parent_functor;
    friend struct transwarp::detail::edges_functor;
    friend struct transwarp::detail::final_visitor;

    // Calls the functor of the given task with the results from the futures.
    // Throws transwarp::task_canceled if the task is canceled.
    static result_type evaluate(transwarp::task<Functor, Tasks...>& task,
                                std::tuple<std::shared_future<typename Tasks::result_type>...> futures) {
        if (*task.canceled_)
            throw transwarp::task_canceled(task.get_node());
        return transwarp::detail::call<result_type>(task.functor_, std::move(futures));
    }

    transwarp::node node_;
    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> parents_;
    bool visited_;
    transwarp::detail::priority_functor packager_;
    std::shared_future<result_type> future_;
    std::shared_ptr<std::atomic_bool> canceled_;
};

// The final task is the very last task in the graph. The final task has no children.
// Depending on how tasks in the graph are arranged they can be run in parallel
// by design if set_parallel is called. If not, all tasks are run sequentially.
// Tasks may run in parallel when they do not depend on each other.
// Note that this class is currently not thread-safe, i.e., all methods
// should be called from the same thread.
template<typename Functor, typename... Tasks>
class final_task : public transwarp::task<Functor, Tasks...>,
                   public transwarp::ifinal_task<typename transwarp::task<Functor, Tasks...>::result_type> {
public:
    // This is the result type of this final task.
    // Getting a compiler error here means that the result types of the parent tasks
    // do not match or cannot be converted into the functor's parameters of this task
    using result_type = typename transwarp::task<Functor, Tasks...>::result_type;

    // A final task is defined by its name (can be empty), a function object, and
    // an arbitrary number of parent tasks
    final_task(std::string name, Functor functor, std::shared_ptr<Tasks>... parents)
    : transwarp::task<Functor, Tasks...>(std::move(name), std::move(functor), std::move(parents)...),
      paused_(false)
    {
        finalize();
    }

    virtual ~final_task() = default;

    // delete copy/move semantics
    final_task(const final_task&) = delete;
    final_task& operator=(const final_task&) = delete;
    final_task(final_task&&) = delete;
    final_task& operator=(final_task&&) = delete;

    // Returns the future associated to the underlying execution
    std::shared_future<result_type> get_future() const override {
        return this->future_;
    }

    // Returns the associated node
    const transwarp::node& get_node() const override {
        return this->node_;
    }

    // If n_threads > 0 then assigns a parallel execution to the final task
    // and all its parent tasks. If n_threads == 0 then the parallel execution
    // is removed.
    void set_parallel(std::size_t n_threads) override {
        if (n_threads > 0) {
            pool_ = transwarp::detail::make_unique<transwarp::detail::thread_pool>(n_threads);
        } else {
            pool_.reset();
        }
    }

    // Schedules the final task and all its parent tasks for execution.
    // The execution is either sequential or in parallel. Complexity is O(n)
    // with n being the number of tasks in the graph
    void schedule() override {
        if (!*this->canceled_) {
            prepare_callbacks();
            if (pool_) { // parallel execution
                for (const auto& callback : callbacks_) {
                    pool_->push(callback);
                }
            } else { // sequential execution
                for (const auto& callback : callbacks_) {
                    while (paused_) {};
                    callback();
                }
            }
        }
    }

    // If enabled then all pending tasks are paused. If running sequentially
    // then a call to schedule will block indefinitely. If running in parallel
    // then a call to schedule will queue up new tasks in the underlying thread
    // pool but not process them. Pausing does not affect currently running tasks.
    void set_pause(bool enabled) override {
        paused_ = enabled;
        if (pool_)
            pool_->set_pause(paused_.load());
    }

    // If enabled then all pending tasks are canceled which will
    // throw transwarp::task_canceled when asking a future for its result.
    // Canceling pending tasks does not affect currently running tasks.
    // As long as cancel is enabled new computations cannot be scheduled.
    void set_cancel(bool enabled) override {
        *this->canceled_ = enabled;
    }

    // Returns the graph of the task structure. This is mainly for visualizing
    // the tasks and their interdependencies. Pass the result into transwarp::make_dot
    // to retrieve a dot-style graph representation for easy viewing.
    const std::vector<transwarp::edge>& get_graph() const override {
        return graph_;
    }

protected:

    // Finalizes the graph of tasks by computing IDs, collecting the packager of
    // each task, populating a vector of edges, etc. The packagers are then
    // sorted by level and ID which ensures that tasks higher in the graph
    // are executed first.
    void finalize() {
        std::size_t id = 0;
        this->canceled_ = std::make_shared<std::atomic_bool>(false);
        transwarp::pass_visitor pass;
        transwarp::detail::final_visitor post_visitor(id, packagers_, this->canceled_, graph_);
        this->visit(pass, post_visitor);
        this->unvisit();
        callbacks_.resize(packagers_.size());
        std::sort(packagers_.begin(), packagers_.end(),
                  std::greater<transwarp::detail::priority_functor>());
    }

    // Calls all packagers and stores the results as callbacks. Every task has
    // a packager which, when called, wraps the task and assigns a new future.
    // Calling the callback will then actually run the functor associated to the
    // task and store the result in the future. The callbacks are dealt with
    // by the schedule function.
    void prepare_callbacks() {
        std::transform(packagers_.begin(), packagers_.end(), callbacks_.begin(),
                       [](const transwarp::detail::priority_functor& f) { return f(); });
    }

    std::atomic_bool paused_;
    std::vector<transwarp::detail::priority_functor> packagers_;
    std::vector<std::function<void()>> callbacks_;
    std::unique_ptr<transwarp::detail::thread_pool> pool_;
    std::vector<transwarp::edge> graph_;
};

// A factory function to create a new task
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(std::string name, Functor functor, std::shared_ptr<Tasks>... parents) {
    return std::make_shared<transwarp::task<Functor, Tasks...>>(std::move(name), std::move(functor), std::move(parents)...);
}

// A factory function to create a new task. Overload for auto-naming
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(Functor functor, std::shared_ptr<Tasks>... parents) {
    return transwarp::make_task("", std::move(functor), std::move(parents)...);
}

// A factory function to create a new final task
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::final_task<Functor, Tasks...>> make_final_task(std::string name, Functor functor, std::shared_ptr<Tasks>... parents) {
    return std::make_shared<transwarp::final_task<Functor, Tasks...>>(std::move(name), std::move(functor), std::move(parents)...);
}

// A factory function to create a new final task. Overload for auto-naming
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::final_task<Functor, Tasks...>> make_final_task(Functor functor, std::shared_ptr<Tasks>... parents) {
    return transwarp::make_final_task("", std::move(functor), std::move(parents)...);
}


} // transwarp
