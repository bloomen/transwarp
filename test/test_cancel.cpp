#include "test.h"

template<typename Functor, typename TaskType>
void cancel_with_schedule_all(int expected, Functor functor, TaskType task_type) {
    std::atomic_bool cont(false);
    auto f0 = [&cont] {
       while (!cont);
       return 42;
    };
    auto task1 = tw::make_task(tw::root, f0);
    auto task2 = tw::make_task(task_type, functor, task1);
    tw::parallel executor(2);
    task2->schedule_all(executor);
    task2->cancel_all(true);
    cont = true;
    REQUIRE_THROWS_AS(task2->future().get(), tw::task_canceled);
    task2->cancel_all(false);
    task2->schedule_all(executor);
    REQUIRE(expected == task2->future().get());
}

TEST_CASE("cancel_with_schedule_all_called_before_in_parallel_and_uncancel") {
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, tw::consume);
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, tw::consume_any);
    cancel_with_schedule_all(13, [] () { return 13; }, tw::wait);
    cancel_with_schedule_all(13, [] () { return 13; }, tw::wait_any);
}

TEST_CASE("cancel_with_schedule_all_called_after") {
    auto f0 = [] { return 42; };
    auto f1 = [] (int x) { return x + 13; };
    auto task1 = tw::make_task(tw::root, f0);
    auto task2 = tw::make_task(tw::consume, f1, task1);
    task2->cancel_all(true);
    tw::sequential executor;
    task2->schedule_all(executor);
    REQUIRE(task2->future().valid());
}

struct functor : tw::functor {

    functor(std::condition_variable& cv, std::mutex& mutex, bool& flag,
            std::atomic_bool& cont, bool& started, bool& ended)
    : cv(cv), mutex(mutex), flag(flag), cont(cont),
      started(started), ended(ended)
    {}

    void operator()() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            flag = true;
        }
        cv.notify_one();
        while (!cont);
        started = true;
        transwarp_cancel_point();
        ended = true;
    }

private:
    std::condition_variable& cv;
    std::mutex& mutex;
    bool& flag;
    std::atomic_bool& cont;
    bool& started;
    bool& ended;
};

TEST_CASE("cancel_task_while_running") {
    tw::parallel exec{1};
    std::condition_variable cv;
    std::mutex mutex;
    bool flag = false;
    std::atomic_bool cont{false};
    bool started = false;
    bool ended = false;
    functor f(cv, mutex, flag, cont, started, ended);
    auto task = tw::make_task(tw::root, f);
    task->schedule(exec);
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&flag] { return flag; });
    }
    task->cancel(true);
    cont = true;
    task->wait();
    REQUIRE(started);
    REQUIRE_FALSE(ended);
    REQUIRE_THROWS_AS(task->get(), tw::task_canceled);
}
