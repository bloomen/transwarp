# transwarp

transwarp is a header-only C++ library for task concurrency. It enables you to define
task dependencies and run those tasks in parallel that do not depend on each other.
Under the hood, a directed acyclic graph is built at compile-time enabling efficient 
traversal and type-safe dependencies.

transwarp is designed for ease of use, portability, and scalability. It is written in 
C++11 and only depends on the standard library. Just copy `src/transwarp.h` 
to your project and off you go!

**Example**

This example creates three tasks and connects them with each other to form
a two-level graph. The tasks are then scheduled twice for computation 
while using 4 threads.
```cpp
#include <fstream>
#include <iostream>
#include "transwarp.h"

double add_em_up(double x, int y) {
    return x + y;
}

int main() {
    double value1 = 13.3;
    int value2 = 42;

    auto compute_something = [&value1] { return value1; };
    auto compute_something_else = [&value2] { return value2; };

    // building the task graph
    auto task1 = transwarp::make_task("something", compute_something);
    auto task2 = transwarp::make_task("something else", compute_something_else);
    auto task3 = transwarp::make_final_task("adder", add_em_up, task1, task2);
    task3->set_parallel(4);  // turns on parallel execution with 4 threads for
                             // tasks that do not depend on each other

    // creating a dot-style graph for visualization
    const auto graph = task3->get_graph();
    std::ofstream("basic_with_three_tasks.dot") << transwarp::make_dot(graph);

    // schedule() can now be called as much as desired. The task graph
    // only has to be built once

    task3->schedule();  // schedules all tasks for execution, assigning a future to each task
    std::cout << "result = " << task3->get_future().get() << std::endl;  // result = 55.3

    // modifying data input
    value1 += 2.5;
    value2 += 1;

    task3->schedule();  // schedules all tasks for execution, replacing the existing futures
    std::cout << "result = " << task3->get_future().get() << std::endl;  // result = 58.8
}
```

The resulting graph of this example looks like this:

![graph](https://raw.githubusercontent.com/bloomen/transwarp/master/examples/basic_with_three_tasks.png)

**Enjoy!**
