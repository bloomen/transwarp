#pragma once
#include <future>
#include <type_traits>
#include <memory>
#include <tuple>
#include "cxxpool.h"


namespace transwarp {
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
            return std::forward<F>(f)(std::get<N>(std::forward<Tuple>(t))->future().get()...);
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
            task->schedule();
        }
    };

    struct reset_functor {
        template<typename Task>
        void operator()(std::shared_ptr<Task> task) const {
            task->reset();
        }
    };

    struct wait_functor {
        template<typename Task>
        void operator()(std::shared_ptr<Task> task) const {
            task->wait();
        }
    };

    struct set_thread_pool_functor {
        explicit set_thread_pool_functor(std::shared_ptr<cxxpool::thread_pool> pool)
        : pool_{std::move(pool)}
        {}
        template<typename Task>
        void operator()(std::shared_ptr<Task> task) const {
            task->pool_ = pool_;
        }
        std::shared_ptr<cxxpool::thread_pool> pool_;
    };

}

template<typename Functor, typename... Tasks>
class task : public std::enable_shared_from_this<task<Functor, Tasks...>> {
public:
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    task(Functor functor, std::shared_ptr<Tasks>... tasks)
    : functor_(std::move(functor)),
      tasks_(std::make_tuple(std::move(tasks)...))
    {}

    void set_parallel(std::size_t n_threads) {
        if (n_threads > 0 && !pool_) {
            pool_ = std::make_shared<cxxpool::thread_pool>(n_threads);
            detail::apply(detail::set_thread_pool_functor(pool_), tasks_);
        }
    }

    void schedule() {
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
    }

    void reset() {
        detail::apply(detail::reset_functor(), tasks_);
        future_ = std::shared_future<result_type>();
    }

    std::shared_future<result_type> future() const {
        return future_;
    }

    void wait() const {
        detail::apply(detail::wait_functor(), tasks_);
        if (future_.valid())
            future_.wait();
    }

private:

    friend struct detail::set_thread_pool_functor;

    static result_type evaluate(std::shared_ptr<task> task) {
        return detail::call<result_type>(task->functor_, task->tasks_);
    }

    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> tasks_;
    std::shared_ptr<cxxpool::thread_pool> pool_;
    std::shared_future<result_type> future_;
};


template<typename Functor, typename... Tasks>
std::shared_ptr<task<Functor, Tasks...>> make_task(Functor functor, std::shared_ptr<Tasks>... tasks) {
    return std::make_shared<task<Functor, Tasks...>>(std::move(functor), std::move(tasks)...);
}

}
