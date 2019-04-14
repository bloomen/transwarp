#include "test.h"

TEST_CASE("schedule_single_task") {
    int x = 13;
    auto task = tw::make_task(tw::root, [&x]{ x *= 2; });
    task->schedule();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_with_three_tasks_sequential") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = tw::make_task(tw::consume, f3, task1, task2);
    task1->schedule();
    task2->schedule();
    task3->schedule();
    REQUIRE(55 == task3->get());
    task3->schedule_all();
    REQUIRE(55 == task3->get());
}

TEST_CASE("schedule_with_three_tasks_parallel") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = tw::make_task(tw::consume, f3, task1, task2);
    tw::parallel executor{4};
    task1->schedule(executor);
    task2->schedule(executor);
    task3->schedule(executor);
    REQUIRE(55 == task3->future().get());
    task3->schedule_all(executor);
    REQUIRE(55 == task3->future().get());
}

TEST_CASE("schedule_with_three_tasks_but_different_schedule") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = tw::make_task(tw::consume, f3, task1, task2);
    task1->schedule();
    task3->schedule_all();
    REQUIRE(55 == task3->future().get());
}

TEST_CASE("schedule_with_three_tasks_wait") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = tw::make_task(tw::wait, f3, task1, task2);

    REQUIRE(tw::task_type::root == task1->type());
    REQUIRE(tw::task_type::root == task2->type());
    REQUIRE(tw::task_type::wait == task3->type());

    task3->schedule_all();
    REQUIRE(17 == task3->future().get());
}

TEST_CASE("schedule_with_three_tasks_wait_any") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = tw::make_task(tw::wait_any, f3, task1, task2);

    REQUIRE(tw::task_type::root == task1->type());
    REQUIRE(tw::task_type::root == task2->type());
    REQUIRE(tw::task_type::wait_any == task3->type());

    task3->schedule_all();
    REQUIRE(17 == task3->future().get());
}

TEST_CASE("schedule_with_three_tasks_consume_any") {
    std::atomic_bool cont(false);
    auto value1 = std::make_shared<int>(42);
    auto f1 = [&cont,value1]() -> int& {
        while (!cont);
        return *value1;
    };
    auto task1 = tw::make_task(tw::root, f1);
    auto value2 = std::make_shared<int>(13);
    auto f2 = [value2]() -> int& { return *value2; }; // to test ref types
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int& x) -> int { return x; };
    auto task3 = tw::make_task(tw::consume_any, f3, task1, task2);

    REQUIRE(tw::task_type::root == task1->type());
    REQUIRE(tw::task_type::root == task2->type());
    REQUIRE(tw::task_type::consume_any == task3->type());

    tw::parallel exec{4};
    task3->schedule_all(exec);
    REQUIRE(13 == task3->future().get());
    cont = true;
}

TEST_CASE("schedule_with_two_tasks_wait_with_void_return") {
    auto f1 = [] {};
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::wait, f2, task1);

    task2->schedule_all();
    REQUIRE(13 == task2->future().get());
}

TEST_CASE("schedule_with_two_tasks_wait_any_with_void_return") {
    auto f1 = [] {};
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::wait_any, f2, task1);

    task2->schedule_all();
    REQUIRE(13 == task2->future().get());
}

TEST_CASE("schedule_with_two_tasks_wait_with_void_return_method_get") {
    auto f1 = [] {};
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::wait, f2, task1);
    task2->schedule_all();
    task1->get();
    REQUIRE(13 == task2->get());
}
