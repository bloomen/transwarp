#pragma once
#include <future>
#include <type_traits>
#include <memory>
#include <tuple>
#include <string>
#include <cstddef>
#include <vector>
#include "cxxpool.h"


namespace transwarp {


struct node {
    std::size_t id;
    std::string name;
};


struct edge {
    node child;
    node parent;
};


namespace detail {

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
    return detail::call_impl<0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::template
            call<Result>(std::forward<F>(f), std::forward<Tuple>(t));
}

template<size_t ...> struct indices
{};

template<size_t ...> struct construct_range;

template< size_t end, size_t idx, size_t ...i >
struct construct_range<end, idx, i... >
     : construct_range<end, idx+1, i..., idx> {};

template< size_t end, size_t ...i >
struct construct_range< end, end, i... > {
    typedef indices< i... > type;
};

template<size_t b, size_t e>
struct index_range {
    typedef typename construct_range<e, b>::type type;
};

template<typename F, typename T, typename ...Args>
void tuple_for_each_index(indices<>, const F&, T&, const Args&...)
{}

template<typename F, typename T, typename ...Args>
void tuple_for_each_index(indices<>, const F&, const T&, const Args&...)
{}

template<size_t i, size_t ...j, typename F, typename T, typename ...Args>
void tuple_for_each_index(indices<i,j...>, const F& f, T& t, const Args&... args) {
    f(std::get<i>(t), args...);
    detail::tuple_for_each_index(indices<j...>(), f, t, args...);
}

template<size_t i, size_t ...j, typename F, typename T, typename ...Args>
void tuple_for_each_index(indices<i,j...>, const F& f, const T& t, const Args&... args) {
    f(std::get<i>(t), args...);
    detail::tuple_for_each_index(indices<j...>(), f, t, args...);
}

template<typename F, typename T, typename ...Args>
void apply(const F& f, T& t, const Args&... args) {
    static const size_t n = std::tuple_size<T>::value;
    typedef typename index_range<0,n>::type index_list;
    detail::tuple_for_each_index(index_list(), f, t, args...);
}

template<typename F, typename T, typename ...Args>
void apply(const F& f, const T& t, const Args&... args) {
    static const size_t n = std::tuple_size<T>::value;
    typedef typename index_range<0,n>::type index_list;
    detail::tuple_for_each_index(index_list(), f, t, args...);
}

struct schedule_functor {
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->make_schedule();
    }
};

struct reset_functor {
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->reset();
    }
};

struct unvisit_functor {
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->unvisit();
    }
};

struct wait_functor {
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->make_wait();
    }
};

struct set_thread_pool_functor {
    explicit set_thread_pool_functor(std::shared_ptr<cxxpool::thread_pool> pool)
    : pool_(std::move(pool))
    {}
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->set_thread_pool(pool_);
    }
    std::shared_ptr<cxxpool::thread_pool> pool_;
};

struct reset_thread_pool_functor {
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->reset_thread_pool();
    }
};

struct make_graph_functor {
    make_graph_functor(std::vector<edge>& graph)
    : graph_(graph)
    {}
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->make_graph(graph_);
    }
    std::vector<edge>& graph_;
};

struct make_edges_functor {
    make_edges_functor(std::vector<edge>& graph, node n)
    : graph_(graph), n_(std::move(n))
    {}
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        graph_.push_back({n_, task->get_node()});
    }
    std::vector<edge>& graph_;
    node n_;
};

struct make_id_functor {
    explicit make_id_functor(std::size_t& id)
    : id_(id)
    {}
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->make_id(id_);
    }
    std::size_t& id_;
};

template<typename PreVisitor, typename PostVisitor>
struct visit_functor {
    visit_functor(PreVisitor* pre_visitor, PostVisitor* post_visitor)
    : pre_visitor_(pre_visitor), post_visitor_(post_visitor)
    {}
    template<typename Task>
    void operator()(std::shared_ptr<Task> task) const {
        task->visit(pre_visitor_, post_visitor_);
    }
    PreVisitor* pre_visitor_;
    PostVisitor* post_visitor_;
};

} // detail


template<typename Functor, typename... Tasks>
class task : public std::enable_shared_from_this<task<Functor, Tasks...>> {
public:
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks)
    : node_{0, std::move(name)},
      functor_(std::move(functor)),
      tasks_(std::make_tuple(std::move(tasks)...)),
      visited_(false)
    {}

    void make_ids() {
        std::size_t id = 0;
        make_id(id);
        unvisit();
    }

    template<typename PreVisitor, typename PostVisitor>
    void visit(PreVisitor* pre_visitor, PostVisitor* post_visitor) {
        if (!visited_) {
            if (pre_visitor)
                pre_visitor(this);
            detail::apply(detail::visit_functor<PreVisitor, PostVisitor>(pre_visitor, post_visitor), tasks_);
            if (post_visitor)
                post_visitor(this);
            visited_ = true;
        }
    }

    void unvisit() const {
        if (visited_) {
            visited_ = false;
            detail::apply(detail::unvisit_functor(), tasks_);
        }
    }

    node get_node() const {
        return node_;
    }

    void set_parallel(std::size_t n_threads) {
        if (n_threads > 0) {
            auto pool = std::make_shared<cxxpool::thread_pool>(n_threads);
            set_thread_pool(std::move(pool));
            unvisit();
        } else {
            reset_thread_pool();
            unvisit();
        }
    }

    void schedule() {
        make_schedule();
        unvisit();
    }

    void wait() const {
        make_wait();
        unvisit();
    }

    std::shared_future<result_type> get_future() const {
        return future_;
    }

    void reset() {
        make_reset();
        unvisit();
    }

    std::vector<edge> get_graph() const {
        std::vector<edge> graph;
        make_graph(graph);
        unvisit();
        return graph;
    }

private:

    friend struct detail::schedule_functor;
    friend struct detail::wait_functor;
    friend struct detail::set_thread_pool_functor;
    friend struct detail::reset_thread_pool_functor;
    friend struct detail::make_graph_functor;
    friend struct detail::make_id_functor;

    static result_type evaluate(std::shared_ptr<task> task) {
        return detail::call<result_type>(task->functor_, task->tasks_);
    }

    void make_id(std::size_t& id) {
        if (!visited_) {
            detail::apply(detail::make_id_functor(id), tasks_);
            node_.id = id++;
            visited_ = true;
        }
    }

    void make_schedule() {
        if (!visited_) {
            detail::apply(detail::schedule_functor(), tasks_);
            if (!future_.valid()) {
                auto self = this->shared_from_this();
                if (pool_) {
                    future_ = pool_->push(&task::evaluate, self);
                } else {
                    auto pkg = std::packaged_task<result_type()>(std::bind(&task::evaluate, self));
                    future_ = pkg.get_future();
                    pkg();
                }
            }
            visited_ = true;
        }
    }

    void make_wait() const {
        if (!visited_) {
            detail::apply(detail::wait_functor(), tasks_);
            if (future_.valid())
                future_.wait();
            visited_ = true;
        }
    }

    void make_graph(std::vector<edge>& graph) const {
        if (!visited_) {
            detail::apply(detail::make_edges_functor(graph, node_), tasks_);
            detail::apply(detail::make_graph_functor(graph), tasks_);
            visited_ = true;
        }
    }

    void set_thread_pool(std::shared_ptr<cxxpool::thread_pool> pool) {
        if (!visited_) {
            pool_ = std::move(pool);
            detail::apply(detail::set_thread_pool_functor(pool_), tasks_);
            visited_ = true;
        }
    }

    void reset_thread_pool() {
        if (!visited_) {
            pool_.reset();
            detail::apply(detail::reset_thread_pool_functor(), tasks_);
            visited_ = true;
        }
    }

    void make_reset() {
        if (!visited_) {
            future_ = std::shared_future<result_type>();
            detail::apply(detail::reset_functor(), tasks_);
            visited_ = true;
        }
    }

    node node_;
    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> tasks_;
    mutable bool visited_;
    std::shared_ptr<cxxpool::thread_pool> pool_;
    std::shared_future<result_type> future_;
};


template<typename Functor, typename... Tasks>
std::shared_ptr<task<Functor, Tasks...>> make_task(std::string name, Functor functor, std::shared_ptr<Tasks>... tasks) {
    return std::make_shared<task<Functor, Tasks...>>(std::move(name), std::move(functor), std::move(tasks)...);
}


} // transwarp
