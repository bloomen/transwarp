#include <fstream>
#include <iostream>
#include "../src/transwarp.h"

namespace examples {

double add_em_up(double x, int y) {
    return x + y;
}

// This example creates three tasks and connects them with each other to form
// a two-level graph. The last task in the graph is then finalized marking it
// as the final task. The tasks are then scheduled twice for computation
// while using 4 threads.
void basic_with_three_tasks(std::ostream& os) {
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
    std::ofstream("basic_with_three_tasks.dot") << transwarp::make_dot(graph);

    // task::schedule() can now be called as much as desired. The task graph
    // only has to be built once

    task3->schedule();  // schedules all tasks for execution, assigning a future to each task
    os << "result = " << task3->get_future().get() << std::endl;  // result = 55.3

    // modifying data input
    value1 += 2.5;
    value2 += 1;

    task3->schedule();  // schedules all tasks for execution, replacing the existing futures
    os << "result = " << task3->get_future().get() << std::endl;  // result = 58.8
}

}

#ifndef USE_LIBUNITTEST
int main() {
    examples::basic_with_three_tasks(std::cout);
}
#endif
