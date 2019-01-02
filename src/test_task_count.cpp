#include "test.h"

TEST_CASE("get_task_count_with_one_task") {
    auto t1 = tw::make_task(tw::root, []{});
    REQUIRE(1 == t1->tasks().size());
}

TEST_CASE("get_task_count_with_one_task_for_value_task") {
    auto t1 = tw::make_value_task(42);
    REQUIRE(1 == t1->tasks().size());
}

TEST_CASE("get_task_count_with_three_tasks") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(43);
    auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
    REQUIRE(1 == t1->tasks().size());
    REQUIRE(1 == t2->tasks().size());
    REQUIRE(3 == t3->tasks().size());
}
