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

// Base class for exceptions
class transwarp_error : public std::runtime_error {
public:
    explicit transwarp_error(const std::string& message)
    : std::runtime_error(message) {}
};

// Exception thrown from a task
class task_error : public transwarp::transwarp_error {
public:
    explicit task_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};

// Exception thrown when a task is canceled
class task_canceled : public transwarp::task_error {
public:
    task_canceled(const transwarp::node& n)
    : transwarp::task_error(n.name + " is canceled") {}
};


namespace detail {

class priority_functor {
public:

    priority_functor() noexcept
    : callback_{}, priority_{}, order_{} {}

    priority_functor(std::function<void()> callback, std::size_t priority, std::size_t order) noexcept
    : callback_{std::move(callback)}, priority_{priority}, order_{order} {}

    bool operator<(const priority_functor& other) const noexcept {
        if (priority_ == other.priority_) {
            return order_ < other.order_;
        } else {
            return priority_ < other.priority_;
        }
    }

    void operator()() const {
        callback_();
    }

private:
    std::function<void()> callback_;
    std::size_t priority_;
    std::size_t order_;
};

class thread_pool_error : public transwarp::transwarp_error {
public:
    explicit thread_pool_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};

class thread_pool {
public:

    explicit thread_pool(std::size_t n_threads)
    : done_{false}, pause_{false}
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
            pause_ = false;
        }
        cond_var_.notify_all();
        for (auto& thread : threads_)
            thread.join();
    }

    void prioritize_threads(std::function<void(std::thread&)>& prioritizer) {
        for (auto& thread : threads_) {
            prioritizer(thread);
        }
    }

    void push(transwarp::detail::priority_functor functor) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (done_)
                throw transwarp::detail::thread_pool_error{"push called while thread pool is shutting down"};
            functors_.push(std::move(functor));
        }
        cond_var_.notify_one();
    }

    void set_pause(bool enabled) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pause_ = enabled;
        }
        if (!pause_)
            cond_var_.notify_all();
    }

private:

    void worker() {
        for (;;) {
            transwarp::detail::priority_functor functor;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_var_.wait(lock, [this]{
                    return !pause_ && (done_ || !functors_.empty());
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
    bool pause_;
    std::vector<std::thread> threads_;
    std::queue<transwarp::detail::priority_functor> functors_;
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

template<size_t ...> struct indices {};

template<size_t ...> struct construct_range;

template< size_t end, size_t idx, size_t ...i >
struct construct_range<end, idx, i...> : construct_range<end, idx+1, i..., idx> {};

template< size_t end, size_t ...i >
struct construct_range< end, end, i... > {
    typedef transwarp::detail::indices< i... > type;
};

template<size_t b, size_t e>
struct index_range {
    typedef typename transwarp::detail::construct_range<e, b>::type type;
};

template<typename F, typename Tuple, typename ...Args>
void tuple_for_each_index(transwarp::detail::indices<>, F&&, Tuple&&, Args&&...) {}

template<size_t i, size_t ...j, typename F, typename Tuple, typename ...Args>
void tuple_for_each_index(transwarp::detail::indices<i,j...>, F&& f, Tuple&& t, Args&&... args) {
    std::forward<F>(f)(std::get<i>(std::forward<Tuple>(t)).get(), std::forward<Args>(args)...);
    transwarp::detail::tuple_for_each_index(transwarp::detail::indices<j...>(),
            std::forward<F>(f), std::forward<Tuple>(t), std::forward<Args>(args)...);
}

template<typename F, typename Tuple, typename ...Args>
void apply(F&& f, Tuple&& t, Args&&... args) {
    using ttype = typename std::decay<Tuple>::type;
    static const std::size_t n = std::tuple_size<ttype>::value;
    typedef typename transwarp::detail::index_range<0, n>::type index_list;
    transwarp::detail::tuple_for_each_index(index_list(),
            std::forward<F>(f), std::forward<Tuple>(t), std::forward<Args>(args)...);
}

template<int offset, typename... Tasks>
struct convert_to_futures_helper {
    static void copy(const std::tuple<std::shared_ptr<Tasks>...>& source, std::tuple<std::shared_future<typename Tasks::result_type>...>& target) {
        std::get<offset>(target) = std::get<offset>(source)->get_future();
        convert_to_futures_helper<offset - 1, Tasks...>::copy(source, target);
    }
};

template<typename... Tasks>
struct convert_to_futures_helper<-1, Tasks...> {
    static void copy(const std::tuple<std::shared_ptr<Tasks>...>&, std::tuple<std::shared_future<typename Tasks::result_type>...>&) {}
};

template<typename... Tasks>
std::tuple<std::shared_future<typename Tasks::result_type>...> convert_to_futures(const std::tuple<std::shared_ptr<Tasks>...>& input) {
    std::tuple<std::shared_future<typename Tasks::result_type>...> result;
    convert_to_futures_helper<static_cast<int>(sizeof...(Tasks)) - 1, Tasks...>::copy(input, result);
    return result;
}

inline std::string trim(const std::string &s, const std::string& chars=" \t\n\r") {
    auto functor = [&chars](char c) { return chars.find(c) != std::string::npos; };
    auto it = std::find_if_not(s.begin(), s.end(), functor);
    return std::string(it, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(it), functor).base());
}

inline void find_levels(const transwarp::node* final) noexcept {
   std::queue<const transwarp::node*> q;
   std::queue<std::size_t> d;
   q.push(final);
   d.push(0);

   while (!q.empty()) {
       const auto current = q.front(); q.pop();
       const auto depth = d.front(); d.pop();

       for (auto n : current->parents) {
           const_cast<transwarp::node*>(n)->level = depth + 1;
           q.push(n);
           d.push(n->level);
       }
   }
}

struct unvisit_functor {
    unvisit_functor() noexcept = default;
    template<typename Task>
    void operator()(Task* task) const noexcept {
        task->unvisit();
    }
};

struct make_edges_functor {
    make_edges_functor(std::vector<transwarp::edge>& graph, transwarp::node& n) noexcept
    : graph_(graph), n_(n) {}
    template<typename Task>
    void operator()(Task* task) const noexcept {
        graph_.push_back({&n_, &task->node_});
    }
    std::vector<transwarp::edge>& graph_;
    transwarp::node& n_;
};

struct make_parents_functor {
    explicit make_parents_functor(transwarp::node& n) noexcept
    : n_(n) {}
    template<typename Task>
    void operator()(Task* task) const noexcept {
        n_.parents.push_back(&task->node_);
    }
    transwarp::node& n_;
};

template<typename PreVisitor, typename PostVisitor>
struct visit_functor {
    visit_functor(PreVisitor& pre_visitor, PostVisitor& post_visitor) noexcept
    : pre_visitor_(pre_visitor), post_visitor_(post_visitor) {}
    template<typename Task>
    void operator()(Task* task) const {
        task->visit(pre_visitor_, post_visitor_);
    }
    PreVisitor& pre_visitor_;
    PostVisitor& post_visitor_;
};

struct final_visitor {
    explicit final_visitor(std::size_t& id) noexcept
    : id_(id) {}
    template<typename Task>
    void operator()(Task* task) const {
        task->node_.id = id_++;
        if (task->node_.name.empty())
            task->node_.name = "task" + std::to_string(task->node_.id);
        if (task->finalized_)
            throw transwarp::task_error("Found already finalized task: " + task->node_.name);
        transwarp::detail::apply(transwarp::detail::make_parents_functor(task->node_), task->tasks_);
    }
    std::size_t& id_;
};

struct cancel_visitor {
    explicit cancel_visitor(bool enabled) noexcept
    : cancel_(enabled) {}
    template<typename Task>
    void operator()(Task* task) const {
        task->cancel_ = cancel_;
    }
    bool cancel_;
};

struct graph_visitor {
    explicit graph_visitor(std::vector<transwarp::edge>& graph) noexcept
    : graph_(graph) {}
    template<typename Task>
    void operator()(Task* task) const noexcept {
        transwarp::detail::apply(transwarp::detail::make_edges_functor(graph_, task->node_), task->tasks_);
    }
    std::vector<transwarp::edge>& graph_;
};

struct callback_visitor {
    explicit callback_visitor(std::priority_queue<transwarp::detail::priority_functor>& queue) noexcept
    : queue_(queue) {}
    template<typename Task>
    void operator()(Task* task) const noexcept {
        auto futures = transwarp::detail::convert_to_futures(task->tasks_);
        auto pack_task = std::make_shared<std::packaged_task<typename Task::result_type()>>(
                              std::bind(&Task::evaluate, task->shared_from_this(), std::move(futures)));
        task->future_ = pack_task->get_future();
        queue_.emplace([pack_task]{ (*pack_task)(); }, task->node_.level, task->node_.id);
    }
    std::priority_queue<transwarp::detail::priority_functor>& queue_;
};

struct set_pool_visitor {
    explicit set_pool_visitor(std::shared_ptr<transwarp::detail::thread_pool> pool) noexcept
    : pool_(std::move(pool)) {}
    template<typename Task>
    void operator()(Task* task) const noexcept {
        task->pool_ = pool_;
    }
    std::shared_ptr<transwarp::detail::thread_pool> pool_;
};

struct reset_pool_visitor {
    reset_pool_visitor() noexcept = default;
    template<typename Task>
    void operator()(Task* task) const {
        task->pool_.reset();
    }
};

} // detail


// A visitor to be used to do nothing
struct pass_visitor {
    pass_visitor() noexcept = default;
    template<typename Task>
    void operator()(Task*) const noexcept {}
};

// Creates a dot-style string from the given graph
inline std::string make_dot(const std::vector<transwarp::edge>& graph) {
    auto info = [](const transwarp::node* n) {
        const auto name = transwarp::detail::trim(n->name);
        return '"' + name + "\nid " + std::to_string(n->id) + " level " + std::to_string(n->level)
                   + " parents " + std::to_string(n->parents.size()) + '"';
    };
    std::string dot = "digraph {\n";
    for (const auto& pair : graph) {
        dot += info(pair.parent) + " -> " + info(pair.child) + '\n';
    }
    dot += "}\n";
    return dot;
}

// An interface for the task class
template<typename ResultType>
class itask {
public:
    virtual ~itask() = default;
    virtual void finalize() = 0;
    virtual void set_parallel(std::size_t n_threads, std::function<void(std::thread&)> thread_prioritizer=nullptr) = 0;
    virtual void schedule() = 0;
    virtual void set_pause(bool enabled) = 0;
    virtual void set_cancel(bool enabled) = 0;
    virtual std::shared_future<ResultType> get_future() const = 0;
    virtual const transwarp::node& get_node() const = 0;
    virtual std::vector<transwarp::edge> get_graph() = 0;
};

// A task representing a piece work given by a functor and parent tasks.
// Depending on how tasks are arranged they can be run in parallel by design
// if set_parallel is called. If not, all tasks are run sequentially.
// Tasks may run in parallel when they do not depend on each other.
// By connecting tasks a directed acyclic graph is built.
// Note that the task class is currently not thread-safe, i.e., all methods
// should be called from the same thread.
template<typename Functor, typename... Tasks>
class task : public transwarp::itask<typename std::result_of<Functor(typename Tasks::result_type...)>::type>,
             public std::enable_shared_from_this<transwarp::task<Functor, Tasks...>> {
public:
    // This is the result type of this task.
    // Getting a compiler error here means that the result types of the parent tasks
    // do not match or cannot be converted into the functor's parameters of this task
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    // A task is defined by its name (can be empty), a function object, and
    // an arbitrary number of parent tasks
    task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks)
    : node_{0, 0, std::move(name), {}},
      functor_(std::move(functor)),
      tasks_(std::make_tuple(std::move(tasks)...)),
      visited_(false),
      finalized_(false),
      cancel_(false),
      pause_(false)
    {}

    // Finalizes the task which must only be called by the final task.
    // The final task does not have any children.
    void finalize() override {
        std::size_t id = 0;
        transwarp::pass_visitor pass;
        transwarp::detail::final_visitor pre_visitor(id);
        visit(pre_visitor, pass);
        unvisit();
        transwarp::detail::find_levels(&node_);
        finalized_ = true;
    }

    // If n_threads > 0 then assigns a parallel execution to the final task
    // and all its parent tasks. If n_threads == 0 then the parallel execution
    // is removed. The thread_prioritizer prioritizes the threads launched.
    // Only to be called by the final task.
    void set_parallel(std::size_t n_threads, std::function<void(std::thread&)> thread_prioritizer=nullptr) override {
        check_is_finalized();
        transwarp::pass_visitor pass;
        if (n_threads > 0) {
            auto pool = std::make_shared<transwarp::detail::thread_pool>(n_threads);
            if (thread_prioritizer)
                pool->prioritize_threads(thread_prioritizer);
            transwarp::detail::set_pool_visitor pre_visitor(std::move(pool));
            visit(pre_visitor, pass);
        } else {
            transwarp::detail::reset_pool_visitor pre_visitor;
            visit(pre_visitor, pass);
        }
        unvisit();
    }

    // Schedules the final task and all its parent tasks for execution.
    // The execution is either sequential or in parallel.
    // Only to be called by the final task.
    void schedule() override {
        check_is_finalized();
        if (!cancel_) {
            transwarp::pass_visitor pass;
            std::priority_queue<transwarp::detail::priority_functor> queue;
            transwarp::detail::callback_visitor post_visitor(queue);
            visit(pass, post_visitor);
            unvisit();
            if (pool_) {
                while (!queue.empty()) {
                    pool_->push(queue.top());
                    queue.pop();
                }
            } else {
                while (!queue.empty()) {
                    while (pause_) {};
                    queue.top()();
                    queue.pop();
                }
            }
        }
    }

    // If enabled then all pending tasks are paused. If running sequentially
    // then a call to schedule will block indefinitely. If running in parallel
    // then a call to schedule will queue up new tasks in the underlying thread
    // pool but not process them. Pausing does not affect currently running tasks.
    // Only to be called by the final task
    void set_pause(bool enabled) override {
        check_is_finalized();
        pause_ = enabled;
        if (pool_)
            pool_->set_pause(pause_);
    }

    // If enabled then all pending tasks are canceled which will
    // throw transwarp::task_canceled when asking a future for its result.
    // Canceling pending tasks does not affect currently running tasks.
    // As long as cancel is enabled new computations cannot be scheduled.
    // Only to be called by the final task
    void set_cancel(bool enabled) override {
        check_is_finalized();
        transwarp::pass_visitor pass;
        transwarp::detail::cancel_visitor pre_visitor(enabled);
        visit(pre_visitor, pass);
        unvisit();
    }

    // Returns the future associated to the underlying execution
    std::shared_future<result_type> get_future() const override {
        return future_;
    }

    // Returns the associated node
    const transwarp::node& get_node() const override {
        return node_;
    }

    // Creates a graph of the task structure. This is mainly for visualizing
    // the tasks and their interdependencies. Pass the result into make_dot_graph
    // to retrieve a dot-style graph representation for easy viewing.
    // Only to be called by the final task.
    std::vector<transwarp::edge> get_graph() override {
        check_is_finalized();
        std::vector<transwarp::edge> graph;
        transwarp::pass_visitor pass;
        transwarp::detail::graph_visitor pre_visitor(graph);
        visit(pre_visitor, pass);
        unvisit();
        return graph;
    }

    // Visits each task in a depth-first traversal. The pre_visitor is called
    // before traversing through parents and the post_visitor after. A visitor
    // takes a pointer to a task (task*) as its only input argument.
    // Only to be called by the final task.
    template<typename PreVisitor, typename PostVisitor>
    void visit(PreVisitor& pre_visitor, PostVisitor& post_visitor) {
        if (!visited_) {
            pre_visitor(this);
            transwarp::detail::apply(transwarp::detail::visit_functor<PreVisitor, PostVisitor>(pre_visitor, post_visitor), tasks_);
            post_visitor(this);
            visited_ = true;
        }
    }

    // Traverses through all tasks and marks them as not visited.
    // Only to be called by the final task.
    void unvisit() {
        if (visited_) {
            visited_ = false;
            transwarp::detail::apply(transwarp::detail::unvisit_functor(), tasks_);
        }
    }

    // Returns the functor
    Functor get_functor() const {
        return functor_;
    }

    // Returns the parent tasks
    std::tuple<std::shared_ptr<Tasks>...> get_tasks() const {
        return tasks_;
    }

private:

    friend struct transwarp::detail::make_edges_functor;
    friend struct transwarp::detail::make_parents_functor;
    friend struct transwarp::detail::reset_pool_visitor;
    friend struct transwarp::detail::set_pool_visitor;
    friend struct transwarp::detail::graph_visitor;
    friend struct transwarp::detail::callback_visitor;
    friend struct transwarp::detail::final_visitor;
    friend struct transwarp::detail::cancel_visitor;

    static result_type evaluate(std::shared_ptr<transwarp::task<Functor, Tasks...>> task,
                                std::tuple<std::shared_future<typename Tasks::result_type>...> futures) {
        if (task->cancel_)
            throw transwarp::task_canceled(task->get_node());
        return transwarp::detail::call<result_type>(task->functor_, std::move(futures));
    }

    void check_is_finalized() const {
        if (!finalized_)
            throw transwarp::task_error("task is not finalized: " + node_.name);
    }

    transwarp::node node_;
    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> tasks_;
    bool visited_;
    bool finalized_;
    std::atomic_bool cancel_;
    bool pause_;
    std::shared_future<result_type> future_;
    std::shared_ptr<transwarp::detail::thread_pool> pool_;
};

// A factory function to easily create new tasks
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks) {
    return std::make_shared<transwarp::task<Functor, Tasks...>>(std::move(name), std::move(functor), std::move(tasks)...);
}

// A factory function to easily create new tasks. Overload for auto-naming
template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(Functor functor, std::shared_ptr<Tasks>... tasks) {
    return make_task("", std::move(functor), std::move(tasks)...);
}


} // transwarp
