#include <fstream>
#include <iostream>
#include "../src/transwarp.h"

namespace tw = transwarp;

namespace examples {

double add_em_up(double x, int y) {
    return x + y;
}

// This example creates three tasks and connects them with each other to form
// a two-level graph. The tasks are then scheduled twice for computation
// while using 4 threads.
void basic_with_three_tasks(std::ostream& os) {
    double value1 = 13.3;
    int value2 = 42;

    auto something = [&value1] { return value1; };
    auto something_else = [&value2] { return value2; };

    // building the task graph
    auto task1 = tw::make_task(tw::root, "something", something);
    auto task2 = tw::make_task(tw::root, "something else", something_else);
    auto task3 = tw::make_task(tw::consume, "adder", add_em_up, task1, task2);

    // creating a dot-style graph for visualization
    const auto graph = task3->get_graph();
    std::ofstream("basic_with_three_tasks.dot") << tw::to_string(graph);

    // schedule() can now be called as much as desired. The task graph
    // only has to be built once

    // parallel execution with 4 threads for independent tasks
    tw::parallel executor(4);

    task3->schedule_all(executor);  // schedules all tasks for execution, assigning a future to each task
    os << "result = " << task3->get_future().get() << std::endl;  // result = 55.3

    // modifying data input
    value1 += 2.5;
    value2 += 1;

    task3->reset_all();  // reset to allow for re-schedule of all tasks

    task3->schedule_all(executor);  // schedules all tasks for execution, assigning new futures
    os << "result = " << task3->get_future().get() << std::endl;  // result = 58.8
}

}

#ifndef USE_LIBUNITTEST
int main() {
    std::cout << "Running example: basic_with_three_tasks ..." << std::endl;
    examples::basic_with_three_tasks(std::cout);
}
#endif
