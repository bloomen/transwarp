#include "test.h"

std::shared_ptr<tw::itask> generic_task() {
    auto task = tw::make_task(tw::root, []{});
    return task;
}

TEST_CASE("sequenced") {
    tw::sequential seq;
    int value = 5;
    auto functor = [&value]{ value *= 2; };
    seq.execute(functor, *generic_task());
    REQUIRE(10 == value);
}

TEST_CASE("parallel") {
    tw::parallel par(4);
    std::atomic_bool done(false);
    int value = 5;
    auto functor = [&value, &done]{ value *= 2; done = true; };
    par.execute(functor, *generic_task());
    while (!done);
    REQUIRE(10 == value);
}

TEST_CASE("schedule_all_without_executor") {
    int x = 13;
    auto task = tw::make_task(tw::root, [&x]{ x *= 2; });
    task->schedule_all();
    task->future().wait();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_all_without_executor_wait_method") {
    int x = 13;
    auto task = tw::make_task(tw::root, [&x]{ x *= 2; });
    REQUIRE_THROWS_AS(task->wait(), tw::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task->get(), tw::transwarp_error); // not scheduled yet
    task->schedule_all();
    task->wait();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_all_with_task_specific_executor") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    task->set_executor(std::make_shared<tw::sequential>());
    task->schedule_all();
    REQUIRE(84 == task->get());
}

TEST_CASE("invalid_task_specific_executor") {
    auto task = tw::make_task(tw::root, []{});
    REQUIRE_THROWS_AS(task->set_executor(nullptr), tw::transwarp_error);
}

TEST_CASE("parallel_with_zero_threads") {
    REQUIRE_THROWS_AS(tw::parallel{0}, tw::invalid_parameter);
}

struct mock_exec : tw::executor {
    bool called = false;
    std::string name() const override {
        return "mock_exec";
    }
    void execute(const std::function<void()>& functor, tw::itask&) override {
        called = true;
        functor();
    }
};

TEST_CASE("set_executor_name_and_reset") {
    auto task = tw::make_task(tw::root, []{});
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    REQUIRE(exec->name() == task->executor()->name());
    task->remove_executor();
    REQUIRE_FALSE(task->executor());
}

TEST_CASE("set_executor_without_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    task->schedule();
    REQUIRE(exec->called);
    REQUIRE(84 == task->future().get());
}

TEST_CASE("set_executor_with_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    tw::sequential exec_seq;
    task->schedule(exec_seq);
    REQUIRE(exec->called);
    REQUIRE(84 == task->future().get());
}

TEST_CASE("remove_executor_with_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = tw::make_task(tw::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    task->remove_executor();
    mock_exec exec_seq;
    task->schedule(exec_seq);
    REQUIRE_FALSE(exec->called);
    REQUIRE(exec_seq.called);
    REQUIRE(84 == task->future().get());
}

TEST_CASE("set_executor_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto exec = std::make_shared<tw::sequential>();
    t2->set_executor_all(exec);
    REQUIRE(t1->executor()->name() == exec->name());
    REQUIRE(t2->executor()->name() == exec->name());
}

TEST_CASE("remove_executor_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto exec = std::make_shared<tw::sequential>();
    t2->set_executor_all(exec);
    REQUIRE(t1->executor()->name() == exec->name());
    REQUIRE(t2->executor()->name() == exec->name());
    t2->remove_executor_all();
    REQUIRE_FALSE(t1->executor());
    REQUIRE_FALSE(t2->executor());
}

TEST_CASE("parallel_exec_with_on_thread_started") {
    std::atomic<std::size_t> count{0};
    {
        tw::parallel exec{4, [&count](const std::size_t index){ count += index; }};
    }
    REQUIRE(6 == count.load());
}
