#include "test.h"

TEST_CASE("task_next") {
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = t1->then(tw::consume, [](int x){ return x + 1; });
    t2->schedule_all();
    REQUIRE(43 == t2->get());
}

TEST_CASE("task_next_with_name") {
    const std::string name = "task";
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = t1->then(tw::consume, [](int x){ return x + 1; })->named(name);
    t2->schedule_all();
    REQUIRE(43 == t2->get());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE(name == *t2->name());
#else
    REQUIRE(!t2->name());
#endif
}

TEST_CASE("value_task_next") {
    auto t1 = tw::make_value_task(42);
    auto t2 = t1->then(tw::consume, [](int x){ return x + 1; });
    t2->schedule_all();
    REQUIRE(43 == t2->get());
}

TEST_CASE("value_task_next_with_name") {
    const std::string name = "task";
    auto t1 = tw::make_value_task(42);
    auto t2 = t1->then(tw::consume, [](int x){ return x + 1; })->named(name);
    t2->schedule_all();
    REQUIRE(43 == t2->get());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE(name == *t2->name());
#else
    REQUIRE(!t2->name());
#endif
}
