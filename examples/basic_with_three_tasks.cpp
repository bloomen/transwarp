#include "basic_with_three_tasks.h"
#include <transwarp.h>
#include <fstream>
#include <iostream>

namespace tw = transwarp;

namespace examples {

// This example creates three tasks and connects them with each other to form
// a two-level graph. The tasks are then scheduled twice for computation
// while using 4 threads.
void basic_with_three_tasks(std::ostream& os) {
    double x = 0;
    int y = 0;

    // Building the task graph
    auto parent1 = tw::make_task(tw::root, [&x]{ return 13.3 + x; })->named("something");
    auto parent2 = tw::make_task(tw::root, [&y]{ return 42 + y; })->named("something else");
    auto child = tw::make_task(tw::consume, [](double a, int b) { return a + b;
                                            }, parent1, parent2)->named("adder");

    tw::parallel executor{4};  // Parallel execution with 4 threads

    child->schedule_all(executor);  // Schedules all tasks for execution
    os << "result = " << child->get() << std::endl;  // result = 55.3

    // Modifying data input
    x += 2.5;
    y += 1;

    child->schedule_all(executor);  // Re-schedules all tasks for execution
    os << "result = " << child->get() << std::endl;  // result = 58.8

    // Creating a dot-style graph for visualization
    std::ofstream{"basic_with_three_tasks.dot"} << tw::to_string(child->edges());
}

}

#ifndef UNITTEST
int main() {
    std::cout << "Running example: basic_with_three_tasks ..." << std::endl;
    examples::basic_with_three_tasks(std::cout);
}
#endif
