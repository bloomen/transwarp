#include <libunittest/all.hpp>
#include "transwarp.h"


COLLECTION(test_transwarp) {

TEST(basic) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = transwarp::make_task(f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = transwarp::make_task(f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 
    auto root_task = transwarp::make_task(f3, task1, task2);

    root_task->set_parallel(4);

    root_task->schedule();
    ASSERT_EQUAL(89, root_task->future().get());

    ++value;

    root_task->reset();
    root_task->schedule();
    ASSERT_EQUAL(91, root_task->future().get());
}

}
