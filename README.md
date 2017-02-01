# transwarp

transwarp is a header-only C++ library for task parallelism.

It is designed for ease of use, portability, and scalability. It is written in 
C++11 and does not have any external dependencies. Just copy _src/transwarp.h_ 
to your project and off you go!

**Example**

```cpp
#include <fstream>
#include <iostream>
#include "transwarp.h"

double compute_something() {
    return 13.3;
}

int compute_something_else() {
    return 42;
}

double add_em_up(double x, int y) {
    return x + y;
}

int main() {
    auto task1 = transwarp::make_task(compute_something);
    auto task2 = transwarp::make_task(compute_something_else);
    auto task3 = transwarp::make_task(add_em_up, task1, task2);
    task3->finalize();  // make task3 the final task
    task3->set_parallel(4);  // turns on parallel execution with 4 threads for 
                             // tasks that do not depend on each other 
    
    const auto graph = task3->get_graph();
    std::ofstream("graph.dot") << transwarp::make_dot_graph(graph);
    
    task3->schedule();  // schedules all tasks for execution
    std::cout << "result = " << task3->get_future().get() << std::endl;
}
```
