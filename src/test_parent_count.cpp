#include "test.h"

TEST_CASE("get_parent_count_with_three_tasks") {
    auto t1 = tw::make_value_task(42);
    auto t2 = tw::make_value_task(43);
    auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
    REQUIRE(0 == t1->parent_count());
    REQUIRE(0 == t2->parent_count());
    REQUIRE(2 == t3->parent_count());
}
