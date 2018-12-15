#include "test.h"

TEST_CASE("transwarp_error") {
    const std::string msg = "text";
    try {
        throw tw::transwarp_error(msg);
    } catch (const std::runtime_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("task_canceled") {
    const std::string msg = "Task canceled: node";
    try {
        throw tw::task_canceled("node");
    } catch (const tw::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("task_destroyed") {
    const std::string msg = "Task destroyed: node";
    try {
        throw tw::task_destroyed("node");
    } catch (const tw::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("invalid_parameter") {
    const std::string msg = "Invalid parameter: param";
    try {
        throw tw::invalid_parameter("param");
    } catch (const tw::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("control_error") {
    const std::string msg = "Control error: msg";
    try {
        throw tw::control_error("msg");
    } catch (const tw::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

void make_test_task_with_exception_thrown(std::size_t threads) {
    auto f1 = [] {
        throw std::logic_error("from f1");
        return 42;
    };
    auto f2 = [] (int x) {
        throw std::logic_error("from f2");
        return x + 13;
    };
    auto f3 = [] (int x) {
        throw std::logic_error("from f3");
        return x + 1;
    };
    auto task1 = tw::make_task(tw::root, f1);
    auto task2 = tw::make_task(tw::consume, f2, task1);

    std::shared_ptr<tw::executor> executor;
    std::shared_ptr<tw::task<int>> task3;
    if (threads > 0) {
        task3 = tw::make_task(tw::consume, f3, task2);
        executor = std::make_shared<tw::parallel>(threads);
    } else {
        task3 = tw::make_task(tw::consume, f3, task2);
        executor = std::make_shared<tw::sequential>();
    }
    task3->schedule_all(*executor);
    try {
        task3->future().get();
        REQUIRE(false);
    } catch (const std::logic_error& e) {
        REQUIRE(std::string("from f1") == e.what());
    }
}

TEST_CASE("task_with_exception_thrown") {
    make_test_task_with_exception_thrown(0);
    make_test_task_with_exception_thrown(1);
    make_test_task_with_exception_thrown(2);
    make_test_task_with_exception_thrown(3);
    make_test_task_with_exception_thrown(4);
}

TEST_CASE("future_throws_task_destroyed") {
    std::shared_future<void> future;
    tw::parallel exec{1};
    std::atomic_bool cont{false};
    {
        auto task1 = tw::make_task(tw::root, [&cont]{
            while (!cont);
        });
        auto task2 = tw::make_task(tw::wait, []{}, task1);
        task2->schedule_all(exec);
        future = task2->future();
    }
    cont = true;
    REQUIRE(future.valid());
    REQUIRE_THROWS_AS(future.get(), tw::task_destroyed);
}
