#include "test.h"


TEST_CASE("timer_schedule_once") {
    auto t = tw::make_task(tw::root, []{
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });
    t->add_listener(std::make_shared<tw::timer>());
    t->schedule();
#ifdef TRANSWARP_DISABLE_TASK_TIME
    REQUIRE(t->avg_idletime_us() == -1);
    REQUIRE(t->avg_waittime_us() == -1);
    REQUIRE(t->avg_runtime_us() == -1);
#else
    REQUIRE(t->avg_idletime_us() >= 0);
    REQUIRE(t->avg_waittime_us() >= 0);
    REQUIRE(t->avg_runtime_us() > 0);
#endif
}

TEST_CASE("timer_schedule_twice") {
    auto t = tw::make_task(tw::root, []{
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });
    t->add_listener(std::make_shared<tw::timer>());
    t->schedule();
    t->schedule();
#ifdef TRANSWARP_DISABLE_TASK_TIME
    REQUIRE(t->avg_idletime_us() == -1);
    REQUIRE(t->avg_waittime_us() == -1);
    REQUIRE(t->avg_runtime_us() == -1);
#else
    REQUIRE(t->avg_idletime_us() >= 0);
    REQUIRE(t->avg_waittime_us() >= 0);
    REQUIRE(t->avg_runtime_us() > 0);
#endif
}

TEST_CASE("timer_schedule_once_but_task_canceled") {
    auto t = tw::make_task(tw::root, []{
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });
    t->add_listener(std::make_shared<tw::timer>());
    t->cancel(true);
    t->schedule();
#ifdef TRANSWARP_DISABLE_TASK_TIME
    REQUIRE(t->avg_idletime_us() == -1);
    REQUIRE(t->avg_waittime_us() == -1);
    REQUIRE(t->avg_runtime_us() == -1);
#else
    REQUIRE(t->avg_idletime_us() >= 0);
    REQUIRE(t->avg_waittime_us() >= 0);
    REQUIRE(t->avg_runtime_us() >= 0);
#endif
}

TEST_CASE("timer_schedule_twice_but_task_canceled") {
    auto t = tw::make_task(tw::root, []{
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });
    t->add_listener(std::make_shared<tw::timer>());
    t->schedule();
    t->cancel(true);
    t->schedule();
#ifdef TRANSWARP_DISABLE_TASK_TIME
    REQUIRE(t->avg_idletime_us() == -1);
    REQUIRE(t->avg_waittime_us() == -1);
    REQUIRE(t->avg_runtime_us() == -1);
#else
    REQUIRE(t->avg_idletime_us() >= 0);
    REQUIRE(t->avg_waittime_us() >= 0);
    REQUIRE(t->avg_runtime_us() > 0);
#endif
}
