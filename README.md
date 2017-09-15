**_Not production ready!_**

# transwarp ![master branch](https://travis-ci.org/bloomen/transwarp.svg?branch=master)](https://travis-ci.org/bloomen/transwarp) ![develop branch](https://travis-ci.org/bloomen/transwarp.svg?branch=develop)](https://travis-ci.org/bloomen/transwarp)

transwarp is a header-only C++ library for task concurrency. It enables you to free
your functors from explicit threads and transparently manage dependencies.
Under the hood, a directed acyclic graph is built at compile-time enabling efficient 
traversal and type-safe dependencies. Once a graph is created its structure
cannot be changed at runtime.

A task in transwarp is defined through a functor, parent tasks, and an optional name. 
A task can either be consuming all or just one of its parents, or simply wait for their 
completion similar to continuations. transwarp supports executors 
either per task or globally when scheduling the tasks in the graph. Executors are
decoupled from tasks and simply provide a way of running a given function.

transwarp is designed for ease of use, portability, and scalability. It is written in 
C++11 and only depends on the standard library. Just copy `src/transwarp.h` 
to your project and off you go! Tested with GCC, Clang, and Visual Studio.

**Table of contents**

  * [Example](#example)
  * [API doc](#api-doc)
     * [Creating tasks](#creating-tasks)
     * [Scheduling tasks](#scheduling-tasks)
     * [More on executors](#more-on-executors)
  * [Running the tests](#running-the-tests)
  * [Feedback](#feedback)

## Example

This example creates three tasks and connects them with each other to form
a two-level graph. The tasks are then scheduled twice for computation 
while using 4 threads.
```cpp
#include <fstream>
#include <iostream>
#include "transwarp.h"

namespace tw = transwarp;

double add_em_up(double x, int y) {
    return x + y;
}

int main() {
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
    tw::parallel executor{4};

    task3->schedule_all(executor);  // schedules all tasks for execution, assigning a future to each task
    std::cout << "result = " << task3->get_future().get() << std::endl;  // result = 55.3

    // modifying data input
    value1 += 2.5;
    value2 += 1;

    task3->schedule_all(executor);  // schedules all tasks for execution, assigning new futures
    std::cout << "result = " << task3->get_future().get() << std::endl;  // result = 58.8
}
```

The resulting graph of this example looks like this:

![graph](https://raw.githubusercontent.com/bloomen/transwarp/master/examples/basic_with_three_tasks.png)

Every bubble represents a task and every arrow an edge between two tasks. 
The first line within a bubble is the task name. The second line denotes the task
type followed by the task id and the number of parents. 

## API doc

This is a brief API doc of transwarp. In the following we will use `tw` as a namespace alias for `transwarp`.

### Creating tasks

transwarp supports five different task types:
```cpp
root,        // The task has no parents
consume,     // The task's functor consumes all parent results
consume_any, // The task's functor consumes the first parent result that becomes ready
wait,        // The task's functor takes no arguments but waits for all parents to finish
wait_any,    // The task's functor takes no arguments but waits for the first parent to finish
```
The task type is passed as the first parameter to `make_task`, e.g., to create 
a `consume` task simply do this:
```cpp
auto task = tw::make_task(tw::consume, functor, parent1, parent2);
```
where `functor` denotes some callable and `parent1/2` the parent tasks. 
Note that `functor` in this case has to accept two arguments that match the 
result types of the parent tasks.

Tasks can be freely chained together using the different task types. 
The only restriction is that tasks without parents have to be labeled as `root` tasks. 

### Scheduling tasks

Once a task is created it can be scheduled just by itself:
```cpp
auto task = tw::make_task(tw::root, functor);
task->schedule();
```
which, if nothing else is specified, will run the task on the current thread. 
However, using the built-in `parallel` executor the task can be pushed into a 
thread pool and executed asynchronously:
```cpp
tw::parallel executor{4};  // thread pool with 4 threads
auto task = tw::make_task(tw::root, functor);
task->schedule(executor);
```
Regardless of how you schedule, the shared future associated to the underlying 
execution can be retrieved through:
```cpp
auto future = task->get_future();
std::cout << future.get() << std::endl;
```  
When chaining multiple tasks together a directed acyclic graph is built in which
every task can be scheduled individually. Though, in many scenarios it is useful
to compute all tasks in the right order with a single call:
```cpp
auto parent1 = tw::make_task(tw::root, foo);  // foo is a functor
auto parent2 = tw::make_task(tw::root, bar);  // bar is a functor
auto task = tw::make_task(tw::consume, functor, parent1, parent2);
task->schedule_all();  // schedules all parents and itself
```
which can also be scheduled using an executor, for instance:
```cpp
tw::parallel executor{4};
task->schedule_all(executor);
```
which will run those tasks in parallel that do not depend on each other.

### More on executors

We have seen that we can pass executors to `schedule()` and `schedule_all()`.
Additionally, they can be assigned to a task directly:
```cpp
auto exec1 = std::make_shared<tw::parallel>(2);
task->set_executor(exec1);
tw::sequential exec2;
task->schedule(exec2);  // exec1 will be used to schedule the task
``` 
The task-specific executor will always be preferred over other executors when
scheduling tasks.

transwarp defines an executor interface which can be implemented to perform custom 
behaviour when scheduling tasks. The interface looks like this:
```cpp
class executor {
public:
    virtual ~executor() = default;
    virtual std::string get_name() const = 0;
    virtual void execute(const std::function<void()>& functor, const std::shared_ptr<tw::node>& node) = 0;
};

``` 
where `functor` denotes the function to be run and `node` an object that holds 
meta-data of the current task.

## Running the tests

You'll need:
* [libunittest](http://libunittest.sourceforge.net/)
* [boost](http://www.boost.org/)
* [cppcheck](http://cppcheck.sourceforge.net/) 
* [valgrind](http://valgrind.org/) 

If you're on Mac or Linux, just do:
```
./make_check.sh [compiler]
```
where compiler defaults to `g++` and can also be `clang++`.

If you're using Visual Studio you will have to set up a project
manually to run all tests.

## Feedback

Contact me if you have any questions or suggestions to make this a better library!
Email me at `chr.blume@gmail.com` or create a GitHub issue.
