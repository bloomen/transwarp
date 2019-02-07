#include "test.h"

using nm = transwarp::detail::node_manip;

#if !defined(__APPLE__) // any_cast not supported on travis
TEST_CASE("node_clone") {
    auto p1 = std::make_shared<tw::node>();
    auto p2 = std::make_shared<tw::node>();

    tw::node n;
    nm::set_id(n, 42);
    nm::set_level(n, 3);
    nm::set_type(n, tw::task_type::consume_any);
    nm::set_name(n, std::make_optional<std::string>("spacko"));
    nm::set_executor(n, std::make_optional<std::string>("blah"));
    nm::add_parent(n, p1);
    nm::add_parent(n, p2);
    nm::set_priority(n, 13);
    nm::set_custom_data(n, std::make_any<double>(53.8));
    nm::set_canceled(n, true);
    nm::set_avg_idletime_us(n, 1);
    nm::set_avg_waittime_us(n, 2);
    nm::set_avg_runtime_us(n, 3);

    auto cloned = n.clone();
    REQUIRE(42 == cloned->id());
    REQUIRE(3 == cloned->level());
    REQUIRE(tw::task_type::consume_any == cloned->type());
    REQUIRE("spacko" == *cloned->name());
    REQUIRE("blah" == *cloned->executor());
    REQUIRE(0 == cloned->parents().size());
    REQUIRE(13 == cloned->priority());
    REQUIRE(53.8 == std::any_cast<double>(cloned->custom_data()));
    REQUIRE(true == cloned->canceled());
    REQUIRE(1 == cloned->avg_idletime_us());
    REQUIRE(2 == cloned->avg_waittime_us());
    REQUIRE(3 == cloned->avg_runtime_us());
}
#endif

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
