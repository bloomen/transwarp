#ifndef TRANSWARP_MINIMUM_TASK_SIZE
#define TRANSWARP_MINIMUM_TASK_SIZE
#endif
#include <transwarp.h>
#include <iostream>

namespace tw = transwarp;

int main() {
    auto task1 = tw::make_task(tw::root, []{});
    std::cout << "task impl size = " << sizeof(*task1) << std::endl;
    auto task2 = tw::make_value_task(42);
    std::cout << "value task size = " << sizeof(*task2) << std::endl;
}
