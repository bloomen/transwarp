#include "test.h"

TEST_CASE("wait_any") {
    int result = 0;
    std::atomic_bool cont(false);
    auto task1 = tw::make_task(tw::root, [&cont, &result] {
        while (!cont);
        result = 42;
    });
    auto task2 = tw::make_task(tw::root, [&result] {
        result = 43;
    });
    auto task3 = tw::make_task(tw::wait_any, [] {}, task1, task2);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    task3->future().wait();
    REQUIRE(43 == result);
    cont = true;
    REQUIRE(task1->canceled());
}

TEST_CASE("wait_any_with_vector_parents") {
    int result = 0;
    std::atomic_bool cont(false);
    auto task1 = tw::make_task(tw::root, [&cont, &result] {
        while (!cont);
        result = 42;
    });
    auto task2 = tw::make_task(tw::root, [&result] {
        result = 43;
    });
    std::vector<std::shared_ptr<tw::task<void>>> parents = {task1, task2};
    auto task3 = tw::make_task(tw::wait_any, [] {}, parents);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    task3->future().wait();
    REQUIRE(43 == result);
    cont = true;
    REQUIRE(task1->canceled());
}

TEST_CASE("wait") {
    int result1 = 0;
    int result2 = 0;
    auto task1 = tw::make_task(tw::root, [&result1] {
        result1 = 42;
    });
    auto task2 = tw::make_task(tw::root, [&result2] {
        result2 = 43;
    });
    auto task3 = tw::make_task(tw::wait, [] {}, task1, task2);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    task3->future().wait();
    REQUIRE(42 == result1);
    REQUIRE(43 == result2);
}

TEST_CASE("wait_with_vector_parents") {
    int result1 = 0;
    int result2 = 0;
    auto task1 = tw::make_task(tw::root, [&result1] {
        result1 = 42;
    });
    auto task2 = tw::make_task(tw::root, [&result2] {
        result2 = 43;
    });
    std::vector<std::shared_ptr<tw::task<void>>> parents = {task1, task2};
    auto task3 = tw::make_task(tw::wait, [] {}, parents);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    task3->future().wait();
    REQUIRE(42 == result1);
    REQUIRE(43 == result2);
}
