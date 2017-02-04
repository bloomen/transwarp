# transwarp

transwarp is a header-only C++ library for task concurrency. It enables you to define
task dependencies and run those tasks in parallel that do not depend on each other.
Under the hood, a directed acyclic graph is built at compile-time enabling efficient 
traversal and type-safe dependencies.

transwarp is designed for ease of use, portability, and scalability. It is written in 
C++11 and only depends on the standard library. Just copy `src/transwarp.h` 
to your project and off you go!

**Example**

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
    auto task3 = transwarp::make_task("adder", add_em_up, task1, task2);
    task3->finalize();  // make task3 the final task
    task3->set_parallel(4);  // turns on parallel execution with 4 threads for 
                             // tasks that do not depend on each other 

    // creating a dot-style graph for visualization
    const auto graph = task3->get_graph();
    std::ofstream("graph.dot") << transwarp::make_dot_graph(graph);

    // task::schedule() can now be called as much as desired. The task graph
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

**Running the tests**

Given [libunittest](http://libunittest.sourceforge.net/) is present you can build the tests using, e.g., GCC:
```
g++ -std=c++11 -pthread -lunittest src/test.cpp -o test
```
Then to run:
```
./test -v
```

**Enjoy!**
