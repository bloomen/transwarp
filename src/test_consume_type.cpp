#include "test.h"

TEST_CASE("consume_any") {
    std::atomic_bool cont(false);
    auto task1 = tw::make_task(tw::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto task2 = tw::make_task(tw::root, [] {
        return 43;
    });
    auto task3 = tw::make_task(tw::consume_any, [](int x) { return x; }, task1, task2);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    REQUIRE(43 == task3->future().get());
    cont = true;
    REQUIRE(task1->canceled());
}

TEST_CASE("consume_any_with_vector_parents") {
    std::atomic_bool cont(false);
    auto task1 = tw::make_task(tw::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto task2 = tw::make_task(tw::root, [] {
        return 43;
    });
    std::vector<std::shared_ptr<tw::task<int>>> parents = {task1, task2};
    auto task3 = tw::make_task(tw::consume_any, [](int x) { return x; }, parents);
    tw::parallel exec{2};
    task3->schedule_all(exec);
    REQUIRE(43 == task3->future().get());
    cont = true;
    REQUIRE(task1->canceled());
}
