#include "test.h"

TEST_CASE("accept_with_one_parent") {
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = tw::make_task(tw::accept, [](std::shared_future<int> p1) { return p1.get(); }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->get());
}

TEST_CASE("accept_with_two_parents") {
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = tw::make_task(tw::root, []{ return 13.3; });
    auto t3 = tw::make_task(tw::accept, [](std::shared_future<int> p1, const std::shared_future<double>& p2) { return p1.get() + p2.get(); }, t1, t2);
    t3->schedule_all();
    REQUIRE(55.3 == t3->get());
}

TEST_CASE("accept_with_two_vector_parents") {
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = tw::make_task(tw::root, []{ return 13; });
    std::vector<std::shared_ptr<tw::task<int>>> parents = {t1, t2};
    auto t3 = tw::make_task(tw::accept, [](std::vector<std::shared_future<int>> p) {
        REQUIRE(2 == p.size());
        return p[0].get() + p[1].get();
    }, parents);
    t3->schedule_all();
    REQUIRE(55 == t3->get());
}

TEST_CASE("accept_any_with_one_parent") {
    auto t1 = tw::make_task(tw::root, []{ return 42; });
    auto t2 = tw::make_task(tw::accept_any, [](std::shared_future<int> p1) { return p1.get(); }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->get());
}

TEST_CASE("accept_any_with_two_parents") {
    std::atomic_bool cont(false);
    auto t1 = tw::make_task(tw::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto t2 = tw::make_task(tw::root, [] {
        return 43;
    });
    auto t3 = tw::make_task(tw::accept_any, [](std::shared_future<int> x) { return x.get(); }, t1, t2);
    tw::parallel exec{2};
    t3->schedule_all(exec);
    REQUIRE(43 == t3->future().get());
    cont = true;
    REQUIRE(t1->canceled());
}

TEST_CASE("accept_any_with_two_vector_parents") {
    std::atomic_bool cont(false);
    auto t1 = tw::make_task(tw::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto t2 = tw::make_task(tw::root, [] {
        return 43;
    });
    std::vector<std::shared_ptr<tw::task<int>>> parents = {t1, t2};
    auto t3 = tw::make_task(tw::accept_any, [](std::shared_future<int> x) { return x.get(); }, parents);
    tw::parallel exec{2};
    t3->schedule_all(exec);
    REQUIRE(43 == t3->future().get());
    cont = true;
    REQUIRE(t1->canceled());
}
