#pragma once
#include <future>
#include <type_traits>
#include <memory>
#include <tuple>


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
    void tuple_for_each_index(indices<i,j...>, const F& f, T& t, const Args&... args)
    {
        f(std::get<i>(t), args...);
        detail::tuple_for_each_index(indices<j...>(), f, t, args...);
    }

    template<size_t i, size_t ...j, typename F, typename T, typename ...Args>
    void tuple_for_each_index(indices<i,j...>, const F& f, const T& t, const Args&... args)
    {
        f(std::get<i>(t), args...);
        detail::tuple_for_each_index(indices<j...>(), f, t, args...);
    }

    template<typename F, typename T, typename ...Args>
    void apply(const F& f, T& t, const Args&... args)
    {
        static const size_t n = std::tuple_size<T>::value;
        typedef typename index_range<0,n>::type index_list;
        detail::tuple_for_each_index(index_list(), f, t, args...);
    }

    template<typename F, typename T, typename ...Args>
    void apply(const F& f, const T& t, const Args&... args)
    {
        static const size_t n = std::tuple_size<T>::value;
        typedef typename index_range<0,n>::type index_list;
        detail::tuple_for_each_index(index_list(), f, t, args...);
    }

}


template<typename Functor, typename... Tasks>
class task : public std::enable_shared_from_this<task<Functor, Tasks...>> {
public:
    using result_type = typename std::result_of<Functor(typename Tasks::result_type...)>::type;

    task(Functor functor, std::shared_ptr<Tasks>... tasks)
    : functor_(std::move(functor)),
      tasks_(std::make_tuple(tasks...))
    {}

    void schedule() {
        detail::apply(schedule_functor(), tasks_);
        auto self = this->shared_from_this();
        pkg_ = std::packaged_task<result_type()>{std::bind(&task::evaluate, self)};
        future_ = pkg_.get_future();
        pkg_(); // do async
    }

    std::shared_future<result_type> future() const {
        return future_;
    }

private:

    struct schedule_functor {
        template<typename Task>
        void operator()(std::shared_ptr<Task> task) const {
            task->schedule();
        }
    };

    static result_type evaluate(std::shared_ptr<task> t) {
        return detail::call<result_type>(std::move(t->functor_), t->tasks_);
    }

    Functor functor_;
    std::tuple<std::shared_ptr<Tasks>...> tasks_;
    std::packaged_task<result_type()> pkg_;
    std::shared_future<result_type> future_;
};


template<typename Functor, typename... Tasks>
std::shared_ptr<task<Functor, Tasks...>> make_task(Functor functor, std::shared_ptr<Tasks>... tasks) {
    return std::make_shared<task<Functor, Tasks...>>(std::move(functor), std::move(tasks)...);
}
