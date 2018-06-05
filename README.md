# transwarp 

**Version 1.6.0**

<a href="https://raw.githubusercontent.com/bloomen/transwarp/master/src/transwarp.h" download="transwarp.h">Download as single header from here</a>

<a href="https://bloomen.github.io/transwarp">Doxygen documentation</a>

[![Gitter](https://badges.gitter.im/bloomen/transwarp.svg)](https://gitter.im/bloomen/transwarp)

transwarp is a header-only C++ library for task concurrency. It
enables you to free your functors from explicit threads and
transparently manage dependencies.  Under the hood, a directed acyclic
graph is built that allows for efficient traversal and type-safe
dependencies. Use transwarp if you want to model your dependent
operations in a graph of tasks and intend to invoke the graph more than
once.

A task in transwarp is defined through a functor, parent tasks, and an
optional name. Chaining tasks creates an acyclic graph. A task can either be 
consuming all or just one of its parents, or simply wait for their completion 
similar to continuations. transwarp supports executors either per task or
globally when scheduling the tasks in the graph. Executors are
decoupled from tasks and simply provide a way of running a given
function.

transwarp is designed for ease of use, portability, and
scalability. It is written in C++11 and only depends on the standard
library. Just copy `src/transwarp.h` to your project and off you go!
Tested with GCC, Clang, and Visual Studio.

**Table of contents**

  * [Build status](#build-status)
  * [Example](#example)
  * [API doc](#api-doc)
     * [Creating tasks](#creating-tasks)
     * [Scheduling tasks](#scheduling-tasks)
     * [Executors](#executors)
     * [Canceling tasks](#canceling-tasks)
     * [Event system] (#event-system)
  * [Feedback](#feedback)

## Build status

The *master* branch is always at the latest release. The *develop* branch is at 
the latest release plus some delta.

GCC/Clang on master [![Travis](https://travis-ci.org/bloomen/transwarp.svg?branch=master)](https://travis-ci.org/bloomen/transwarp/branches) and develop [![Travis](https://travis-ci.org/bloomen/transwarp.svg?branch=develop)](https://travis-ci.org/bloomen/transwarp/branches)

Visual Studio on master [![Appveyor](https://ci.appveyor.com/api/projects/status/wrtbk9l3b94eeb9t/branch/master?svg=true)](https://ci.appveyor.com/project/bloomen/transwarp?branch=master) and develop [![Appveyor](https://ci.appveyor.com/api/projects/status/wrtbk9l3b94eeb9t/branch/develop?svg=true)](https://ci.appveyor.com/project/bloomen/transwarp?branch=develop)

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

    // building the task graph
    auto task1 = tw::make_value_task("something", 13.3);
    auto task2 = tw::make_value_task("something else", 42);
    auto task3 = tw::make_task(tw::consume, "adder", add_em_up, task1, task2);

    // creating a dot-style graph for visualization
    const auto graph = task3->get_graph();
    std::ofstream("basic_with_three_tasks.dot") << tw::to_string(graph);

    // schedule() can now be called as much as desired. The task graph
    // only has to be built once

    // parallel execution with 4 threads for independent tasks
    tw::parallel executor{4};

    task3->schedule_all(executor);  // schedules all tasks for execution
    std::cout << "result = " << task3->get() << std::endl;  // result = 55.3

    // modifying data input
    task1->set_value(15.8);
    task2->set_value(43);

    task3->schedule_all(executor);  // re-schedules all tasks for execution
    std::cout << "result = " << task3->get() << std::endl;  // result = 58.8
}
```

The resulting graph of this example looks like this:

![graph](https://raw.githubusercontent.com/bloomen/transwarp/master/examples/basic_with_three_tasks.png)

Every bubble represents a task and every arrow an edge between two tasks. 
The first line within a bubble is the task name. The second line denotes the task
type followed by the task id and the task level in the graph.

## API doc

This is a brief API doc of transwarp. 
For more details check out the <a href="https://bloomen.github.io/transwarp">doxygen documentation</a>
and the <a href="https://github.com/bloomen/transwarp/tree/master/examples">transwarp examples</a>.

In the following we will use `tw` as a namespace alias for `transwarp`.

### Creating tasks

transwarp supports seven different task types:
```cpp
root,        // The task has no parents
accept,      // The task's functor accepts all parent futures
accept_any,  // The task's functor accepts the first parent future that becomes ready
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
The only restriction is that tasks without parents have to be either labeled as `root` tasks
or defined as value tasks. 

The `accept` and `accept_any` types give you the greatest flexibility but require your
functor to take `std::shared_future<T>` types. The `consume` and `consume_any` task types, however, 
require your functor to take the direct result types of the parent tasks. 

If you have a task that doesn't require a functor and should only ever return a given
value or throw an exception then a value task can be used:
```cpp
auto task = tw::make_value_task(42);  
```
A call to `task->get()` will now always return 42. Note that a value or exception
can also be explicitely assigned to regular tasks by calling the `set_value`
or `set_exception` functions. Value or exception will persist for as long as
the task hasn't been reset by a call to `reset()`.

As shown above, parents can be enumerated as arguments to `make_task` but
can also be collected into a `std::vector` and then be passed to `make_task`, e.g.:
```cpp
std::vector<std::shared_ptr<tw::task<int>>> parents = {parent1, parent2};
auto task = tw::make_task(tw::wait, []{}, parents);
```
This will simply wait for both parents to finish before running the given functor.

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
Regardless of how you schedule, the task result can be retrieved through:
```cpp
std::cout << task->get() << std::endl;
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

### Executors

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
behavior when scheduling tasks. The interface looks like this:
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

### Canceling tasks

A task can be canceled by calling `task->cancel(true)` which will, by default, 
only affect tasks that are not currently running yet. However, if you create a functor
that inherits from `transwarp::functor` you can terminate tasks while they're
running. `transwarp::functor` looks like this:
```cpp

class functor {
public:
    virtual ~functor() = default;

protected:
    /// The node associated to the task
    const std::shared_ptr<transwarp::node>& transwarp_node() const noexcept {
        return transwarp_node_;
    }

    /// If the associated task is canceled then this will throw transwarp::task_canceled
    /// which will stop the task while it's running
    void transwarp_cancel_point() const {
        if (transwarp_node_->is_canceled()) {
            throw transwarp::task_canceled(std::to_string(transwarp_node_->get_id()));
        }
    }
private:
	...
};
```
By placing calls to `transwarp_cancel_point()` in strategic places of your functor
you can denote well defined points where the functor will exit when the associated task is canceled.

As mentioned above, tasks can be explicitly canceled on client request. In addition,
all tasks considered abandoned by `accept_any`, `consume_any`, or `wait_any`
operations are also canceled in ordered to terminate them as soon as their results
become unnecessary. 

### Event system

Transwarp provides an event system that allows you to subscribe to all or specific
events of a task, such as, before started or after finished events. The task events
are enumerated in the `event_type` enum:
```cpp
enum class event_type {
    before_scheduled, // just before a task is scheduled
    before_started,   // just before a task starts running
    after_finished,   // just after a task has finished running
}
```
Listeners are created by sub-classing from the `listener` interface:
```cpp
class listener {
public:
    virtual ~listener() = default;

    /// This may be called from arbitrary threads depending on the event type
    virtual void handle_event(tw::event_type event, const std::shared_ptr<tw::node>& node) = 0;
};
```
A listener can then be passed to the `add_listener` functions of a task
to add a new listener or to the `remove_listener` functions to remove
an existing listener.

## Feedback

Contact me if you have any questions or suggestions to make this a better library!
You can post on [gitter](https://gitter.im/bloomen/transwarp), submit a pull request,
create a Github issue, or simply email me at `chr.blume@gmail.com`.

If you're serious about contributing code to transwarp (which would be awesome!) then 
please submit a pull request and keep in mind that:
- all new development happens on the _develop_ branch while the _master_ branch is at the latest release
- unit tests should be added for all new code by extending the existing unit test suite
- C++ code uses spaces throughout 
