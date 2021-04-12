#include "test.h"

TEST_CASE("invalid_parent_task") {
    auto parent = tw::make_task(tw::root, [] { return 42; });
    parent.reset();
    REQUIRE_THROWS_AS(tw::make_task(tw::consume, [](int) {}, parent), tw::transwarp_error);
}

TEST_CASE("task_with_const_reference_return_type") {
    auto value = std::make_shared<const int>(42);
    auto functor = [value]() -> const int& { return *value; };
    auto task = tw::make_task(tw::root, functor);
    task->schedule();
    REQUIRE(*value == task->future().get());
    REQUIRE(*value == task->get());
}

TEST_CASE("task_with_reference_return_type") {
    auto value = std::make_shared<int>(42);
    auto functor = [value]() -> int& { return *value; };
    auto task = tw::make_task(tw::root, functor);
    task->schedule();
    REQUIRE(*value == task->future().get());
    REQUIRE(*value == task->get());
}

struct non_move_functor {
    non_move_functor() = default;
    non_move_functor(const non_move_functor&) = default;
    non_move_functor& operator=(const non_move_functor&) = default;
    non_move_functor(non_move_functor&&) = delete;
    non_move_functor& operator=(non_move_functor&&) = delete;
    int operator()() const {
        return 43;
    }
};

TEST_CASE("make_task_with_non_move_functor") {
    non_move_functor functor;
    auto task = tw::make_task(tw::root, functor);
    task->schedule();
    REQUIRE(43 == task->future().get());
}

TEST_CASE("make_task_std_function") {
    std::function<int()> functor = [] { return 44; };
    auto task = tw::make_task(tw::root, functor);
    task->schedule();
    REQUIRE(44 == task->future().get());
}

int myfunc() {
    return 45;
}

TEST_CASE("make_task_raw_function") {
    auto task = tw::make_task(tw::root, myfunc);
    task->schedule();
    REQUIRE(45 == task->future().get());
}

template<typename T>
void make_test_pass_by_reference() {
    using data_t = typename std::decay<T>::type;
    auto data = std::make_shared<data_t>();
    const auto data_ptr = data.get();

    auto t1 = tw::make_task(tw::root, [data]() -> T { return *data; });
    auto t2 = tw::make_task(tw::consume, [](T d) -> T { return d; }, t1);
    auto t3 = tw::make_task(tw::consume_any, [](T d) -> T { return d; }, t2);
    t3->schedule_all();

    auto& result = t3->future().get();
    const auto result_ptr = &result;
    REQUIRE(data_ptr == result_ptr);

    auto& result2 = t3->get();
    const auto result2_ptr = &result2;
    REQUIRE(data_ptr == result2_ptr);
}

TEST_CASE("pass_by_reference") {
    using data_t = std::array<double, 10>;
    make_test_pass_by_reference<const data_t&>();
    make_test_pass_by_reference<data_t&>();
}

TEST_CASE("make_task_from_base_task") {
    std::shared_ptr<tw::task<int>> t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = tw::make_task(tw::consume, [](int x){ return x; }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->future().get());
}
