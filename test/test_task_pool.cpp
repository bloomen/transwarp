#include "test.h"


TEST_CASE("task_pool_constructor") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 3, 100);
    REQUIRE(3 == pool.size());
    REQUIRE(3 == pool.idle_count());
    REQUIRE(0 == pool.busy_count());
    REQUIRE(3 == pool.minimum_size());
    REQUIRE(100 == pool.maximum_size());
}

TEST_CASE("task_pool_constructor_overload") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 3, 5);
    REQUIRE(3 == pool.size());
    REQUIRE(3 == pool.idle_count());
    REQUIRE(0 == pool.busy_count());
    REQUIRE(3 == pool.minimum_size());
    REQUIRE(5 == pool.maximum_size());
}

TEST_CASE("task_pool_constructor_throws_for_invalid_minimum") {
    auto lambda = []{ tw::task_pool<int>{tw::make_task(tw::root, []{ return 42; }), 0, 100}; };
    REQUIRE_THROWS_AS(lambda(), tw::invalid_parameter);
}

TEST_CASE("task_pool_constructor_throws_for_invalid_minimum_maximum") {
    auto lambda = []{ tw::task_pool<int>{tw::make_task(tw::root, []{ return 42; }), 3, 2}; };
    REQUIRE_THROWS_AS(lambda(), tw::invalid_parameter);
}

TEST_CASE("task_pool_next_task") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 2, 100);
    REQUIRE(2 == pool.size());
    auto g1 = pool.next_task();
    REQUIRE(g1);
    REQUIRE(1 == pool.idle_count());
    REQUIRE(1 == pool.busy_count());
    auto g2 = pool.next_task();
    REQUIRE(g2);
    REQUIRE(0 == pool.idle_count());
    REQUIRE(2 == pool.busy_count());
    auto g3 = pool.next_task();
    REQUIRE(g3);
    REQUIRE(1 == pool.idle_count());
    REQUIRE(3 == pool.busy_count());
    REQUIRE(4 == pool.size());
    auto g4 = pool.next_task();
    REQUIRE(g4);
    REQUIRE(0 == pool.idle_count());
    REQUIRE(4 == pool.busy_count());
    REQUIRE(4 == pool.size());
    g1->schedule();
    g2->schedule();
    g3->schedule();
    g4->schedule();
    auto g5 = pool.next_task();
    REQUIRE(g5);
    REQUIRE(3 == pool.idle_count());
    REQUIRE(1 == pool.busy_count());
    REQUIRE(4 == pool.size());
}

TEST_CASE("task_pool_next_task_with_nullptr") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 1, 2);
    REQUIRE(1 == pool.size());
    auto g1 = pool.next_task();
    REQUIRE(g1);
    REQUIRE(0 == pool.idle_count());
    REQUIRE(1 == pool.busy_count());
    auto g2 = pool.next_task();
    REQUIRE(g2);
    REQUIRE(0 == pool.idle_count());
    REQUIRE(2 == pool.busy_count());
    auto g3 = pool.next_task();
    REQUIRE_FALSE(g3); // got a nullptr
    REQUIRE(0 == pool.idle_count());
    REQUIRE(2 == pool.busy_count());
}

TEST_CASE("task_pool_resize") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 2, 100);
    REQUIRE(2 == pool.size());
    pool.resize(4);
    REQUIRE(4 == pool.size());
    pool.resize(1);
    REQUIRE(2 == pool.size());
}

TEST_CASE("task_pool_resize_with_max") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 2, 5);
    REQUIRE(2 == pool.size());
    pool.resize(6);
    REQUIRE(5 == pool.size());
}

TEST_CASE("task_pool_reclaim") {
    tw::task_pool<int> pool(tw::make_task(tw::root, []{ return 42; }), 2, 4);
    REQUIRE(2 == pool.size());
    auto g1 = pool.next_task();
    REQUIRE(g1);
    auto g2 = pool.next_task();
    REQUIRE(g2);
    auto g3 = pool.next_task();
    REQUIRE(g3);
    auto g4 = pool.next_task();
    REQUIRE(g4);
    REQUIRE(4 == pool.size());
    pool.resize(2);
    REQUIRE(4 == pool.size());
    g1->schedule();
    g2->schedule();
    g3->schedule();
    g4->schedule();
    pool.resize(2); // calls reclaim
    REQUIRE(2 == pool.size());
}

TEST_CASE("task_pool_compute") {
    auto t1 = tw::make_task(tw::root, []{ return 1; });
    auto t2 = tw::make_task(tw::root, []{ return 2; });
    auto t3 = tw::make_task(tw::consume, [](int x, int y){ return x + y; }, t1, t2);
    tw::task_pool<int> pool(t3, 2, 4);
    auto g1 = pool.next_task();
    REQUIRE(g1);
    g1->schedule_all();
    REQUIRE(3 == g1->get());
}
