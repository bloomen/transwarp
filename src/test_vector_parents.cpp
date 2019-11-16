#include "test.h"

TEST_CASE("make_task_accept_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::accept,
    [](std::vector<std::shared_future<int>> parents) {
        REQUIRE(2 == parents.size());
        return parents[0].get() + parents[1].get();
    }, vec);
    t->schedule();
    REQUIRE(55 == t->get());
}

TEST_CASE("make_task_accept_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::accept,
    [](std::vector<std::shared_future<int>> parents) {
        REQUIRE(2 == parents.size());
        return parents[0].get() + parents[1].get();
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(55 == t->get());
}

TEST_CASE("make_task_accept_any_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::accept_any,
    [](std::shared_future<int> parent) {
        return parent.get();
    }, vec);
    t->schedule();
    REQUIRE((t->get() == 42 || t->get() == 13));
}

TEST_CASE("make_task_accept_any_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::accept_any,
    [](std::shared_future<int> parent) {
        return parent.get();
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE((t->get() == 42 || t->get() == 13));
}

TEST_CASE("make_task_consume_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [](std::vector<int> parents) {
        REQUIRE(2 == parents.size());
        return parents[0] + parents[1];
    }, vec);
    t->schedule();
    REQUIRE(55 == t->get());
}

TEST_CASE("make_task_consume_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [](std::vector<int> parents) {
        REQUIRE(2 == parents.size());
        return parents[0] + parents[1];
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(55 == t->get());
}

TEST_CASE("make_task_consume_with_vector_and_void_result") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [](std::vector<int> parents) {
        REQUIRE(2 == parents.size());
    }, vec);
    t->schedule();
    REQUIRE(t->has_result());
}

TEST_CASE("make_task_consume_with_vector_and_name_and_void_result") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [](std::vector<int> parents) {
        REQUIRE(2 == parents.size());
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(t->has_result());
}

TEST_CASE("make_task_consume_with_vector_and_ref_result") {
    int res = 10;
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [&res](std::vector<int> parents) -> int& {
        REQUIRE(2 == parents.size());
        return res;
    }, vec);
    t->schedule();
    REQUIRE(res == t->get());
}

TEST_CASE("make_task_consume_with_vector_and_name_and_ref_result") {
    int res = 10;
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume,
    [&res](std::vector<int> parents) -> int& {
        REQUIRE(2 == parents.size());
        return res;
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(res == t->get());
}

TEST_CASE("make_task_consume_any_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume_any,
    [](int parent) {
        return parent;
    }, vec);
    t->schedule();
    REQUIRE((t->get() == 42 || t->get() == 13));
}

TEST_CASE("make_task_consume_any_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::consume_any,
    [](int parent) {
        return parent;
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE((t->get() == 42 || t->get() == 13));
}

TEST_CASE("make_task_wait_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::wait,
    []() {
        return 1;
    }, vec);
    t->schedule();
    REQUIRE(1 == t->get());
}

TEST_CASE("make_task_wait_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::wait,
    []() {
        return 1;
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(1 == t->get());
}

TEST_CASE("make_task_wait_any_with_vector") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::wait_any,
    []() {
        return 1;
    }, vec);
    t->schedule();
    REQUIRE(1 == t->get());
}

TEST_CASE("make_task_wait_any_with_vector_and_name") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(13);
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {t1, t2};
    auto t = tw::make_task(tw::wait_any,
    []() {
        return 1;
    }, vec)->named("task");
    t->schedule();
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("task" == *t->name());
#else
    REQUIRE(!t->name());
#endif
    REQUIRE(1 == t->get());
}

TEST_CASE("make_task_with_empty_vector_parents") {
    const std::vector<std::shared_ptr<tw::task<int>>> vec = {};
    auto t_accept = tw::make_task(tw::accept, no_op_func{}, vec);
    REQUIRE(tw::task_type::root == t_accept->type());
    auto t_accept_any = tw::make_task(tw::accept_any, no_op_func{}, vec);
    REQUIRE(tw::task_type::root == t_accept_any->type());
    auto t_consume = tw::make_task(tw::consume, no_op_func{}, vec);
    REQUIRE(tw::task_type::root == t_consume->type());
    auto t_consume_any = tw::make_task(tw::consume_any, no_op_func{}, vec);
    REQUIRE(tw::task_type::root == t_consume_any->type());
    auto t_wait = tw::make_task(tw::wait, [](){}, vec);
    REQUIRE(tw::task_type::root == t_wait->type());
    auto t_wait_any = tw::make_task(tw::wait_any, [](){}, vec);
    REQUIRE(tw::task_type::root == t_wait_any->type());
}
