#include "test.h"

TEST_CASE("value_task") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    REQUIRE(42 == t->future().get());
    REQUIRE(t->was_scheduled());
    REQUIRE(t->is_ready());
    REQUIRE(t->edges().empty());
    auto n = t;
    REQUIRE(0u == n->id());
    REQUIRE(tw::task_type::root == n->type());
    REQUIRE(!n->name());
    REQUIRE(!n->executor());
    REQUIRE(n->parents().empty());
    REQUIRE(0 == n->priority());
    REQUIRE(!any_data_ok(n->custom_data()));
    REQUIRE(!n->canceled());
}

TEST_CASE("value_task_with_name") {
    const std::string name = "albert";
    auto t = tw::make_value_task(42)->named(name);
    REQUIRE(42 == t->get());
    REQUIRE(42 == t->future().get());
    REQUIRE(t->was_scheduled());
    REQUIRE(t->is_ready());
    REQUIRE(t->edges().empty());
    auto n = t;
    REQUIRE(0u == n->id());
    REQUIRE(tw::task_type::root == n->type());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE(name == *n->name());
#else
    REQUIRE(!n->name());
#endif
    REQUIRE(!n->executor());
    REQUIRE(n->parents().empty());
    REQUIRE(0 == n->priority());
    REQUIRE(!any_data_ok(n->custom_data()));
    REQUIRE(!n->canceled());
}

TEST_CASE("value_task_with_priority_and_custom_data") {
    auto t = tw::make_value_task(42);
    t->set_priority(13);
    auto data = std::make_shared<double>(13.5);
    t->set_custom_data(data);
    auto n = t;
#ifndef TRANSWARP_DISABLE_TASK_PRIORITY
    REQUIRE(13 == n->priority());
#else
    REQUIRE(0 == n->priority());
#endif
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(13.5 == *get_any_data<std::shared_ptr<double>>(n->custom_data()));
#else
    REQUIRE(!any_data_ok(n->custom_data()));
#endif
    t->remove_custom_data();
    t->reset_priority();
    REQUIRE(0 == n->priority());
    REQUIRE(!any_data_ok(n->custom_data()));
}

TEST_CASE("value_task_with_priority_all_and_custom_data_all") {
    auto t = tw::make_value_task(42);
    t->set_priority_all(13);
    auto data = std::make_shared<double>(13.5);
    t->set_custom_data_all(data);
    auto n = t;
#ifndef TRANSWARP_DISABLE_TASK_PRIORITY
    REQUIRE(13 == n->priority());
#else
    REQUIRE(0 == n->priority());
#endif
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(13.5 == *get_any_data<std::shared_ptr<double>>(n->custom_data()));
#else
    REQUIRE(!any_data_ok(n->custom_data()));
#endif
    t->remove_custom_data_all();
    t->reset_priority_all();
    REQUIRE(0 == n->priority());
    REQUIRE(!any_data_ok(n->custom_data()));
}

TEST_CASE("value_task_in_a_graph") {
    auto t1 = tw::make_value_task(42);
    REQUIRE(42 == t1->get());
    auto t2 = tw::make_value_task(13.3);
    REQUIRE(13.3 == t2->get());
    auto t3 = tw::make_task(tw::consume, [](int x, double y) { return x + y; }, t1, t2);
    t3->schedule();
    REQUIRE(55.3 == t3->get());
}

TEST_CASE("value_task_and_executor") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    auto exec = std::make_shared<tw::sequential>();
    t->set_executor(exec);
    REQUIRE(!t->executor());
    t->remove_executor();
    REQUIRE(!t->executor());
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_executor_all") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    auto exec = std::make_shared<tw::sequential>();
    t->set_executor_all(exec);
    REQUIRE(!t->executor());
    t->remove_executor_all();
    REQUIRE(!t->executor());
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_schedule") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    tw::sequential exec;
    t->schedule(true);
    t->schedule(exec, true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_schedule_all") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    tw::sequential exec;
    t->schedule_all(true);
    t->schedule_all(exec, true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_wait") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    t->wait();
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_reset_and_cancel") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    t->reset();
    t->cancel(true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_reset_all_and_cancel_all") {
    auto t = tw::make_value_task(42);
    REQUIRE(42 == t->get());
    t->reset_all();
    t->cancel_all(true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_with_lvalue_reference") {
    const int x = 42;
    auto t = tw::make_value_task(x);
    REQUIRE(x == t->get());
}

TEST_CASE("value_task_with_rvalue_reference") {
    int x = 42;
    auto t = tw::make_value_task(std::move(x));
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_with_changing_value") {
    int x = 42;
    auto t = tw::make_value_task(x);
    x = 43;
    REQUIRE(42 == t->get());
}

TEST_CASE("make_ready_future_with_value") {
    const int x = 42;
    auto future = tw::detail::make_future_with_value<int>(x);
    REQUIRE(x == future.get());
}

TEST_CASE("make_ready_future") {
    auto future = tw::detail::make_ready_future();
    REQUIRE(future.valid());
}

TEST_CASE("make_ready_future_with_exception") {
    std::runtime_error e{"42"};
    auto future = tw::detail::make_future_with_exception<int>(std::make_exception_ptr(e));
    try {
        future.get();
    } catch (std::runtime_error& e) {
        REQUIRE(std::string{"42"} == e.what());
        return;
    }
    throw std::runtime_error{"shouldn't get here"};
}

TEST_CASE("make_ready_future_with_invalid_exception") {
    REQUIRE_THROWS_AS(tw::detail::make_future_with_exception<int>(std::exception_ptr{}), tw::transwarp_error);
}

TEST_CASE("task_set_value_and_remove_value") {
    const int x = 42;
    const int y = 55;
    auto t = tw::make_task(tw::root, [x]{ return x; });
    t->schedule();
    REQUIRE(x == t->get());
    t->set_value(y);
    REQUIRE(t->is_ready());
    REQUIRE(y == t->get());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), tw::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_set_value_and_remove_value_for_mutable_ref") {
    int x = 42;
    int y = 55;
    auto t = tw::make_task(tw::root, [x]{ return x; });
    t->schedule();
    REQUIRE(x == t->get());
    t->set_value(y);
    REQUIRE(t->is_ready());
    REQUIRE(y == t->get());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), tw::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_set_value_and_remove_value_for_void") {
    auto t = tw::make_task(tw::root, []{});
    t->schedule();
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), tw::transwarp_error);
    t->set_value();
    REQUIRE(t->is_ready());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), tw::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
}

TEST_CASE("task_set_exception_and_remove_exception") {
    const int x = 42;
    auto t = tw::make_task(tw::root, [x]{ return x; });
    std::runtime_error e{"blah"};
    t->set_exception(std::make_exception_ptr(e));
    REQUIRE(t->is_ready());
    REQUIRE_THROWS_AS(t->get(), std::runtime_error);
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), tw::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_pass_exception_to_set_value") {
    auto t = tw::make_task(tw::root, []{ return std::make_exception_ptr(std::runtime_error{"foo"}); });
    std::runtime_error e{"blah"};
    t->set_value(std::make_exception_ptr(e));
    const auto result = t->get();
    try {
        std::rethrow_exception(result);
    } catch (const std::runtime_error& re) {
        REQUIRE(std::string(e.what()) == std::string(re.what()));
        return;
    }
    throw std::runtime_error{"shouldn't get here"};
}

TEST_CASE("has_result") {
    auto t = tw::make_task(tw::root, []{});
    REQUIRE(!t->has_result());
    t->schedule();
    REQUIRE(t->has_result());
}

TEST_CASE("has_result_for_value_task") {
    auto t = tw::make_value_task(42);
    REQUIRE(t->has_result());
}

TEST_CASE("value_task_with_volatile_int") {
    volatile int x = 42;
    auto t = tw::make_value_task(x);
    REQUIRE(x == t->get());
    t->set_value(43);
    REQUIRE(43 == t->get());
    x *= 2;
    t->set_value(x);
    REQUIRE(x == t->get());
}
