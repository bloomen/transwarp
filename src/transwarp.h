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


// TODOs
// - write tests
// - avoid pause/resume of pool (need to push tasks in level order)


namespace transwarp {


struct node {
    std::size_t id;
    std::size_t level;
    std::string name;
    std::vector<node*> parents;
};


struct edge {
    transwarp::node child;
    transwarp::node parent;
};


class transwarp_error : public std::runtime_error {
public:
    explicit transwarp_error(const std::string& message)
    : std::runtime_error(message) {}
};


class task_error : public transwarp::transwarp_error {
public:
    explicit task_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};


class thread_pool_error : public transwarp::transwarp_error {
public:
    explicit thread_pool_error(const std::string& message)
    : transwarp::transwarp_error(message) {}
};


namespace detail {

template<typename Index, Index max = std::numeric_limits<Index>::max()>
class infinite_counter {
public:
    infinite_counter()
    : count_{0} {}

    infinite_counter& operator++() {
        if (count_.back() == max)
            count_.push_back(0);
        else
            ++count_.back();
        return *this;
    }

    bool operator>(const infinite_counter& other) const {
        if (count_.size() == other.count_.size()) {
            return count_.back() > other.count_.back();
        } else {
            return count_.size() > other.count_.size();
        }
    }

private:
    std::vector<Index> count_;
};

class priority_task {
public:
    typedef std::size_t counter_elem_t;

    priority_task()
    : callback_{}, priority_{}, order_{} {}

    priority_task(std::function<void()> callback, std::size_t priority,
                  transwarp::detail::infinite_counter<counter_elem_t> order)
    : callback_{std::move(callback)}, priority_(priority), order_{std::move(order)} {}

    bool operator<(const priority_task& other) const {
        if (priority_ == other.priority_) {
            return order_ > other.order_;
        } else {
            return priority_ < other.priority_;
        }
    }

    std::function<void()> callback() const {
        return callback_;
    }

private:
    std::function<void()> callback_;
    std::size_t priority_;
    transwarp::detail::infinite_counter<counter_elem_t> order_;
};

class thread_pool {
public:
    /**
     * Constructor. Launches the desired number of threads
     * @param n_threads The number of threads to launch.
     */
    explicit thread_pool(std::size_t n_threads)
    : done_{false}, paused_{false}, threads_{}, tasks_{}, task_counter_{},
      task_cond_var_{}, task_mutex_{}, thread_mutex_{}, thread_prioritizer_{[](std::thread&) {}}
    {
        if (n_threads > 0) {
            std::lock_guard<std::mutex> thread_lock(thread_mutex_);
            const auto n_target = threads_.size() + n_threads;
            while (threads_.size() < n_target) {
                threads_.emplace_back(&thread_pool::worker, this);
                thread_prioritizer_(threads_.back());
            }
        }
    }
    /**
     * Destructor. Joins all threads launched. Waits for all running tasks
     * to complete
     */
    ~thread_pool() {
        {
            std::lock_guard<std::mutex> task_lock(task_mutex_);
            done_ = true;
            paused_ = false;
        }
        task_cond_var_.notify_all();
        std::lock_guard<std::mutex> thread_lock(thread_mutex_);
        for (auto& thread : threads_)
            thread.join();
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;
    /**
     * Sets the thread prioritizer to be used when launching new threads.
     * By default, threads will be launched with the default OS priority
     */
    void set_thread_prioritizer(std::function<void(std::thread&)> prioritizer) {
        thread_prioritizer_ = std::move(prioritizer);
    }
    /**
     * Returns the number of threads launched
     */
    std::size_t n_threads() const {
        {
            std::lock_guard<std::mutex> task_lock(task_mutex_);
            if (done_)
                throw transwarp::thread_pool_error{"n_threads called while thread pool is shutting down"};
        }
        std::lock_guard<std::mutex> thread_lock(thread_mutex_);
        return threads_.size();
    }
    /**
     * Pushes a new task into the thread pool. The task will have a priority of 0
     * @param functor The functor to call
     * @param args The arguments to pass to the functor when calling it
     * @return The future associated to the underlying task
     */
    template<typename Functor, typename... Args>
    auto push(Functor&& functor, Args&&... args) -> std::future<decltype(functor(args...))> {
        return push(0, std::forward<Functor>(functor), std::forward<Args>(args)...);
    }
    /**
     * Pushes a new task into the thread pool while providing a priority
     * @param priority A task priority. Higher priorities are processed first
     * @param functor The functor to call
     * @param args The arguments to pass to the functor when calling it
     * @return The future associated to the underlying task
     */
    template<typename Functor, typename... Args>
    auto push(std::size_t priority, Functor&& functor, Args&&... args) -> std::future<decltype(functor(args...))> {
        typedef decltype(functor(args...)) result_type;
        auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
                std::bind(std::forward<Functor>(functor), std::forward<Args>(args)...));
        auto future = pack_task->get_future();
        {
            std::lock_guard<std::mutex> task_lock(task_mutex_);
            if (done_)
                throw transwarp::thread_pool_error{"push called while thread pool is shutting down"};
            tasks_.emplace([pack_task]{ (*pack_task)(); }, priority, ++task_counter_);
        }
        task_cond_var_.notify_one();
        return future;
    }
    /**
     * Returns the current number of queued tasks
     */
    std::size_t n_tasks() const {
        std::lock_guard<std::mutex> lock(task_mutex_);
        return tasks_.size();
    }
    /**
     * Clears all queued tasks. Not affecting currently running tasks
     */
    void clear() {
        std::lock_guard<std::mutex> lock(task_mutex_);
        decltype(tasks_) empty;
        tasks_.swap(empty);
    }
    /**
     * Pauses the processing of tasks. Not affecting currently running tasks
     */
    void pause() {
        std::lock_guard<std::mutex> lock(task_mutex_);
        paused_ = true;
    }
    /**
     * Resumes the processing of tasks
     */
    void resume() {
        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            paused_ = false;
        }
        task_cond_var_.notify_all();
    }

private:

    void worker() {
        for (;;) {
            transwarp::detail::priority_task task;
            {
                std::unique_lock<std::mutex> task_lock(task_mutex_);
                task_cond_var_.wait(task_lock, [this]{
                    return !paused_ && (done_ || !tasks_.empty());
                });
                if (done_ && tasks_.empty())
                    break;
                task = tasks_.top();
                tasks_.pop();
            }
            task.callback()();
        }
    }

    bool done_;
    bool paused_;
    std::vector<std::thread> threads_;
    std::priority_queue<transwarp::detail::priority_task> tasks_;
    transwarp::detail::infinite_counter<typename
    transwarp::detail::priority_task::counter_elem_t> task_counter_;
    std::condition_variable task_cond_var_;
    mutable std::mutex task_mutex_;
    mutable std::mutex thread_mutex_;
    std::function<void(std::thread&)> thread_prioritizer_;
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
        return std::forward<F>(f)(std::get<N>(std::forward<Tuple>(t))->get_future().get()...);
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
void tuple_for_each_index(transwarp::detail::indices<>, F&&, Tuple&&, Args&&...)
{}

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

inline
std::string trim(const std::string &s, const std::string& chars=" \t\n\r") {
    auto functor = [&chars](char c) { return chars.find(c) != std::string::npos; };
    auto it = std::find_if_not(s.begin(), s.end(), functor);
    return std::string(it, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(it), functor).base());
}

inline
void find_levels(transwarp::node* final) {
   std::queue<transwarp::node*> q;
   std::queue<std::size_t> d;
   q.push(final);
   d.push(0);

   while (!q.empty()) {
       const auto current = q.front(); q.pop();
       const auto depth = d.front(); d.pop();

       for (auto n : current->parents) {
           n->level = depth + 1;
           q.push(n);
           d.push(n->level);
       }
   }
}

struct unvisit_functor {
    template<typename Task>
    void operator()(Task* task) const {
        task->unvisit();
    }
};

struct make_edges_functor {
    make_edges_functor(std::vector<transwarp::edge>& graph, transwarp::node n)
    : graph_(graph), n_(std::move(n)) {}
    template<typename Task>
    void operator()(Task* task) const {
        graph_.push_back({n_, task->get_node()});
    }
    std::vector<transwarp::edge>& graph_;
    transwarp::node n_;
};

struct make_parents_functor {
    explicit make_parents_functor(transwarp::node& n)
    : n_(n) {}
    template<typename Task>
    void operator()(Task* task) const {
        n_.parents.push_back(&task->node_);
    }
    transwarp::node& n_;
};

template<typename PreVisitor, typename PostVisitor>
struct visit_functor {
    visit_functor(PreVisitor& pre_visitor, PostVisitor& post_visitor)
    : pre_visitor_(pre_visitor), post_visitor_(post_visitor) {}
    template<typename Task>
    void operator()(Task* task) const {
        task->visit(pre_visitor_, post_visitor_);
    }
    PreVisitor& pre_visitor_;
    PostVisitor& post_visitor_;
};

struct final_visitor {
    explicit final_visitor(std::size_t& id)
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

struct schedule_visitor {
    template<typename Task>
    void operator()(Task* task) const {
        auto self = task->shared_from_this();
        if (task->pool_) {
            task->future_ = task->pool_->push(task->node_.level, &Task::evaluate, self);
        } else {
            auto pkg = std::packaged_task<typename Task::result_type()>(std::bind(&Task::evaluate, self));
            task->future_ = pkg.get_future();
            pkg();
        }
    }
};

struct graph_visitor {
    explicit graph_visitor(std::vector<transwarp::edge>& graph)
    : graph_(graph) {}
    template<typename Task>
    void operator()(Task* task) const {
        transwarp::detail::apply(transwarp::detail::make_edges_functor(graph_, task->node_), task->tasks_);
    }
    std::vector<transwarp::edge>& graph_;
};

struct set_pool_visitor {
    explicit set_pool_visitor(std::shared_ptr<transwarp::detail::thread_pool> pool)
    : pool_(std::move(pool)) {}
    template<typename Task>
    void operator()(Task* task) const {
        task->pool_ = pool_;
    }
    std::shared_ptr<transwarp::detail::thread_pool> pool_;
};

struct reset_pool_visitor {
    template<typename Task>
    void operator()(Task* task) const {
        task->pool_.reset();
    }
};

} // detail


struct pass_visitor {
    template<typename Task>
    void operator()(Task* task) const {}
};


inline
std::string make_dot_graph(const std::vector<transwarp::edge>& graph, const std::string& name="transwarp") {
    auto info = [](transwarp::node n) {
        const auto name = transwarp::detail::trim(n.name);
        return '"' + name + "\nid " + std::to_string(n.id) + " level " + std::to_string(n.level)
                   + " parents " + std::to_string(n.parents.size()) + '"';
    };
    std::string dot = "digraph " + name + " {\n";
    for (const auto& pair : graph) {
        dot += info(pair.parent) + " -> " + info(pair.child) + '\n';
    }
    dot += "}\n";
    return dot;
}


template<typename Functor, typename... Tasks>
class task : public std::enable_shared_from_this<transwarp::task<Functor, Tasks...>> {
public:
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks)
    : node_{0, 0, std::move(name), {}},
      functor_(std::move(functor)),
      tasks_(std::make_tuple(std::move(tasks)...)),
      visited_(false),
      finalized_(false)
    {}

    void finalize() {
        std::size_t id = 0;
        transwarp::pass_visitor pass;
        transwarp::detail::final_visitor post_visitor(id);
        visit(pass, post_visitor);
        unvisit();
        transwarp::detail::find_levels(&node_);
        finalized_ = true;
    }

    void set_parallel(std::size_t n_threads, std::function<void(std::thread&)> thread_prioritizer=nullptr) {
        check_is_finalized();
        transwarp::pass_visitor pass;
        wait_for_completion();
        if (n_threads > 0) {
            auto pool = std::make_shared<transwarp::detail::thread_pool>(n_threads);
            if (thread_prioritizer)
                pool->set_thread_prioritizer(std::move(thread_prioritizer));
            transwarp::detail::set_pool_visitor pre_visitor(std::move(pool));
            visit(pre_visitor, pass);
        } else {
            transwarp::detail::reset_pool_visitor pre_visitor;
            visit(pre_visitor, pass);
        }
        unvisit();
    }

    void schedule() {
        check_is_finalized();
        transwarp::pass_visitor pass;
        transwarp::detail::schedule_visitor pre_visitor;
        wait_for_completion();
        if (pool_) pool_->pause();
        visit(pre_visitor, pass);
        if (pool_) pool_->resume();
        unvisit();
    }

    std::shared_future<result_type> get_future() const {
        return future_;
    }

    const transwarp::node& get_node() const {
        return node_;
    }

    const Functor& get_functor() const {
        return functor_;
    }

    const std::tuple<std::shared_ptr<Tasks>...>& get_tasks() const {
        return tasks_;
    }

    std::vector<transwarp::edge> get_graph() {
        check_is_finalized();
        std::vector<transwarp::edge> graph;
        transwarp::pass_visitor pass;
        transwarp::detail::graph_visitor pre_visitor(graph);
        visit(pre_visitor, pass);
        unvisit();
        return graph;
    }

    template<typename PreVisitor, typename PostVisitor>
    void visit(PreVisitor& pre_visitor, PostVisitor& post_visitor) {
        if (!visited_) {
            pre_visitor(this);
            transwarp::detail::apply(transwarp::detail::visit_functor<PreVisitor, PostVisitor>(pre_visitor, post_visitor), tasks_);
            post_visitor(this);
            visited_ = true;
        }
    }

    void unvisit() {
        if (visited_) {
            visited_ = false;
            transwarp::detail::apply(transwarp::detail::unvisit_functor(), tasks_);
        }
    }

private:

    friend struct transwarp::detail::make_parents_functor;
    friend struct transwarp::detail::reset_pool_visitor;
    friend struct transwarp::detail::set_pool_visitor;
    friend struct transwarp::detail::graph_visitor;
    friend struct transwarp::detail::schedule_visitor;
    friend struct transwarp::detail::final_visitor;

    static result_type evaluate(std::shared_ptr<transwarp::task<Functor, Tasks...>> task) {
        return transwarp::detail::call<result_type>(task->functor_, task->tasks_);
    }

    void check_is_finalized() const {
        if (!finalized_)
            throw transwarp::task_error("task is not finalized: " + node_.name);
    }

    void wait_for_completion() const {
        if (future_.valid())
            future_.wait();
    }

    transwarp::node node_;
    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> tasks_;
    bool visited_;
    bool finalized_;
    std::shared_ptr<transwarp::detail::thread_pool> pool_;
    std::shared_future<result_type> future_;
};


template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks) {
    return std::make_shared<transwarp::task<Functor, Tasks...>>(std::move(name), std::move(functor), std::move(tasks)...);
}


template<typename Functor, typename... Tasks>
std::shared_ptr<transwarp::task<Functor, Tasks...>> make_task(Functor functor, std::shared_ptr<Tasks>... tasks) {
    return make_task("", std::move(functor), std::move(tasks)...);
}


} // transwarp
