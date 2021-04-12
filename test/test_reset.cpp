#include "test.h"

TEST_CASE("reset") {
    int value = 42;
    auto functor = [&value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    task->schedule(false);
    REQUIRE(84 == task->future().get());
    value = 43;
    task->schedule(false);
    REQUIRE(84 == task->future().get());
    task->reset();
    task->schedule(false);
    REQUIRE(86 == task->future().get());
}

TEST_CASE("reset_through_schedule") {
    int value = 42;
    auto functor = [&value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    task->schedule();
    REQUIRE(84 == task->future().get());
    value = 43;
    task->schedule(false);
    REQUIRE(84 == task->future().get());
    task->schedule();
    REQUIRE(86 == task->future().get());
}

TEST_CASE("reset_all") {
    int value = 42;
    auto task = tw::make_task(tw::root, [&value] { return value * 2; });
    auto task2 = tw::make_task(tw::consume, [](int x) { return x + 3; }, task);
    task2->schedule_all(false);
    REQUIRE(84 == task->future().get());
    REQUIRE(87 == task2->future().get());
    value = 43;
    task2->schedule_all(false);
    REQUIRE(84 == task->future().get());
    REQUIRE(87 == task2->future().get());
    task2->reset_all();
    task2->schedule_all(false);
    REQUIRE(86 == task->future().get());
    REQUIRE(89 == task2->future().get());
}

TEST_CASE("reset_all_through_schedule_all") {
    int value = 42;
    auto task = tw::make_task(tw::root, [&value] { return value * 2; });
    auto task2 = tw::make_task(tw::consume, [](int x) { return x + 3; }, task);
    task2->schedule_all();
    REQUIRE(84 == task->future().get());
    REQUIRE(87 == task2->future().get());
    value = 43;
    task2->schedule_all(false);
    REQUIRE(84 == task->future().get());
    REQUIRE(87 == task2->future().get());
    task2->schedule_all();
    REQUIRE(86 == task->future().get());
    REQUIRE(89 == task2->future().get());
}
