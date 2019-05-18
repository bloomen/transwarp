#include "test.h"

constexpr int n_events = static_cast<int>(tw::event_type::count);

struct mock_listener : tw::listener {
    std::vector<tw::event_type> events;
    void handle_event(tw::event_type event, tw::itask&) override {
        events.push_back(event);
    }
};

TEST_CASE("add_remove_listener") {
    auto t = tw::make_task(tw::root, []{});
    auto l1 = std::make_shared<mock_listener>();
    auto l2 = std::make_shared<mock_listener>();
    t->add_listener(l1);
    t->add_listener(l1);
    t->add_listener(l2);
    REQUIRE(1 + 2*n_events == l1.use_count());
    REQUIRE(1 + n_events == l2.use_count());
    t->remove_listener(l1);
    REQUIRE(1 == l1.use_count());
    t->remove_listener(l1);
    REQUIRE(1 == l1.use_count());
    t->remove_listener(l2);
    REQUIRE(1 == l2.use_count());
}

TEST_CASE("add_remove_listener_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::root, []{});
    auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
    auto l1 = std::make_shared<mock_listener>();
    t3->add_listener_all(l1);
    REQUIRE(1 + 3*n_events == l1.use_count());
    t3->remove_listener_all(l1);
    REQUIRE(1 == l1.use_count());
}

TEST_CASE("add_remove_listener_per_event_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::root, []{});
    auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
    auto l1 = std::make_shared<mock_listener>();
    t3->add_listener_all(transwarp::event_type::before_started, l1);
    REQUIRE(1 + 3 == l1.use_count());
    t3->remove_listener_all(transwarp::event_type::after_finished, l1);
    REQUIRE(1 + 3 == l1.use_count());
    t3->remove_listener_all(transwarp::event_type::before_started, l1);
    REQUIRE(1 == l1.use_count());
}

TEST_CASE("add_remove_listeners_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::root, []{});
    auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
    auto l1 = std::make_shared<mock_listener>();
    t3->add_listener_all(l1);
    REQUIRE(1 + 3*n_events == l1.use_count());
    t3->remove_listeners_all(transwarp::event_type::after_finished);
    REQUIRE(1 + 3*(n_events-1) == l1.use_count());
    t3->remove_listeners_all();
    REQUIRE(1 == l1.use_count());
}

TEST_CASE("scheduled_event") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_scheduled == l->events[0]);
}

template<typename Functor>
void test_finished_event(Functor functor) {
    auto t = tw::make_task(tw::root, functor);
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::after_finished == l->events[n_events-3]);
    l->events.clear();
    auto exec = std::make_shared<tw::sequential>();
    t->schedule(*exec);
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::after_finished == l->events[n_events-3]);
    l->events.clear();
    t->set_executor(exec);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::after_finished == l->events[n_events-3]);
}

TEST_CASE("finished_event") {
    test_finished_event([]{});
}

TEST_CASE("finished_event_with_exception") {
    test_finished_event([]{ throw std::bad_alloc{}; });
}

template<typename Functor>
void test_started_event(Functor functor) {
    auto t = tw::make_task(tw::root, functor);
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_started == l->events[2]);
    l->events.clear();
    auto exec = std::make_shared<tw::sequential>();
    t->schedule(*exec);
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_started == l->events[2]);
    l->events.clear();
    t->set_executor(exec);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_started == l->events[2]);
}

TEST_CASE("started_event") {
    test_started_event([]{});
}

TEST_CASE("started_event_with_exception") {
    test_started_event([]{ throw std::bad_alloc{}; });
}

TEST_CASE("canceled_event") {
    auto t = tw::make_task(tw::root, []{ throw tw::task_canceled(""); });
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->schedule();
    REQUIRE(n_events-1 == l->events.size());
    REQUIRE(tw::event_type::before_scheduled == l->events[0]);
    REQUIRE(tw::event_type::after_future_changed == l->events[1]);
    REQUIRE(tw::event_type::before_started == l->events[2]);
    REQUIRE(tw::event_type::before_invoked == l->events[3]);
    REQUIRE(tw::event_type::after_canceled == l->events[4]);
    REQUIRE(tw::event_type::after_finished == l->events[5]);
}

TEST_CASE("add_listener_with_event") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(tw::event_type::before_scheduled, l);
    t->schedule();
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::before_scheduled == l->events[0]);
}

TEST_CASE("remove_listener_with_event") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    REQUIRE(n_events+1 == l.use_count());
    t->remove_listener(tw::event_type::before_scheduled, l);
    REQUIRE(n_events == l.use_count());
    t->schedule();
    REQUIRE(4 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
    REQUIRE(tw::event_type::before_started == l->events[1]);
    REQUIRE(tw::event_type::before_invoked == l->events[2]);
    REQUIRE(tw::event_type::after_finished == l->events[3]);
}

TEST_CASE("remove_listeners_with_event") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    REQUIRE(n_events+1 == l.use_count());
    t->remove_listeners(tw::event_type::before_scheduled);
    REQUIRE(n_events == l.use_count());
    t->schedule();
    REQUIRE(4 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
    REQUIRE(tw::event_type::before_started == l->events[1]);
    REQUIRE(tw::event_type::before_invoked == l->events[2]);
    REQUIRE(tw::event_type::after_finished == l->events[3]);
}

TEST_CASE("remove_listeners") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    REQUIRE(n_events+1 == l.use_count());
    t->remove_listeners();
    REQUIRE(1 == l.use_count());
    t->schedule();
    REQUIRE(0 == l->events.size());
}

template<typename Functor>
void test_invoked_event(Functor functor) {
    auto t = tw::make_task(tw::root, functor);
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_invoked == l->events[3]);
    l->events.clear();
    auto exec = std::make_shared<tw::sequential>();
    t->schedule(*exec);
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_invoked == l->events[3]);
    l->events.clear();
    t->set_executor(exec);
    t->schedule();
    REQUIRE(n_events-2 == l->events.size());
    REQUIRE(tw::event_type::before_invoked == l->events[3]);
}

TEST_CASE("invoked_event") {
    test_invoked_event([]{});
}

TEST_CASE("invoked_event_with_exception") {
    test_invoked_event([]{ throw std::bad_alloc{}; });
}

TEST_CASE("after_custom_data_set_event") {
    auto t = tw::make_task(tw::root, []{});
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->set_custom_data(std::make_any<int>(42));
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_custom_data_set == l->events[0]);
#else
    REQUIRE(0 == l->events.size());
#endif
}

TEST_CASE("after_custom_data_set_event_for_value_task") {
    auto t = tw::make_value_task(0);
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->set_custom_data(std::make_any<int>(42));
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_custom_data_set == l->events[0]);
#else
    REQUIRE(0 == l->events.size());
#endif
}

TEST_CASE("after_future_changed_event") {
    auto t = tw::make_task(tw::root, []{ return 0; });
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->set_value(42);
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
    l->events.clear();
    t->set_exception(std::make_exception_ptr(std::bad_alloc{}));
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
}

TEST_CASE("after_future_changed_event_for_value_task") {
    auto t = tw::make_value_task(0);
    auto l = std::make_shared<mock_listener>();
    t->add_listener(l);
    t->set_value(42);
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
    l->events.clear();
    t->set_exception(std::make_exception_ptr(std::bad_alloc{}));
    REQUIRE(1 == l->events.size());
    REQUIRE(tw::event_type::after_future_changed == l->events[0]);
}
