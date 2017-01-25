#include <libunittest/all.hpp>
#include "transwarp.h"

int value = 42;

int f1() {
    return value;
}

int f2(int v) {
    return v + 2;
}

int f3(int v, int w) {
    return v + w + 3;
}

TEST(initial) {
    auto t1 = make_task(f1);
    auto t2 = make_task(f2, t1);
    auto t3 = make_task(f3, t1, t2);

    t3->set_parallel(4);

    t3->schedule();
    std::cout<<t3->future().get()<<std::endl;

    ++value;
    t3->reset();
    t3->schedule();
    std::cout<<t3->future().get()<<std::endl;
}
