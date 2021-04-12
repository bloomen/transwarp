#include "test.h"

TEST_CASE("task_clone") {
    auto p1 = tw::make_task(tw::root, []{ return 42; });
    auto p2 = tw::make_task(tw::consume, [](int x){ return x + 13; }, p1);
    auto t = tw::make_task(tw::consume, [](int x, int y){ return x + y; }, p1, p2);
    t->schedule_all();
    REQUIRE(97 == t->get());
    auto cloned = t->clone();
    cloned->schedule_all();
    REQUIRE(97 == cloned->get());
    REQUIRE(tw::to_string(t->edges()) == tw::to_string(cloned->edges()));
}
