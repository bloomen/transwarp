#include "basic_with_three_tasks.h"
#include "../src/transwarp.h"
#include <fstream>
#include <iostream>

namespace tw = transwarp;

namespace examples {

double adder(double x, int y) {
    return x + y;
}

// This example creates three tasks and connects them with each other to form
// a two-level graph. The tasks are then scheduled twice for computation
// while using 4 threads.
void basic_with_three_tasks(std::ostream& os) {

    // Building the task graph
    auto task1 = tw::make_value_task("something", 13.3);
    auto task2 = tw::make_value_task("something else", 42);
    auto task3 = tw::make_task(tw::consume, "adder", adder, task1, task2);

    // Creating a dot-style graph for visualization
    const auto graph = task3->get_graph();
    std::ofstream("basic_with_three_tasks.dot") << tw::to_string(graph);

    // schedule() can now be called as much as desired. The task graph
    // only has to be built once

    // Parallel execution with 4 threads for independent tasks
    tw::parallel executor(4);

    task3->schedule_all(executor);  // Schedules all tasks for execution
    os << "result = " << task3->get() << std::endl;  // result = 55.3

    // Modifying data input
    task1->set_value(15.8);
    task2->set_value(43);

    task3->schedule_all(executor);  // Re-schedules all tasks for execution
    os << "result = " << task3->get() << std::endl;  // result = 58.8
}

}

#ifndef UNITTEST
int main() {
    std::cout << "Running example: basic_with_three_tasks ..." << std::endl;
    examples::basic_with_three_tasks(std::cout);
}
#endif
