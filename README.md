# transwarp 

**Version 1.1.0**

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
     * [More on executors](#more-on-executors)
  * [Comparison to other libraries](#comparison-to-other-libraries)
     * [Standard library](#standard-library)
     * [Boost](#boost)
     * [HPX](#hpx)
     * [TBB](#tbb)
     * [Stlab](#stlab)
     * [Conclusions](#conclusions)
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

    task3->schedule_all(executor);  // schedules all tasks for execution, assigning a future to each
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

## Comparison to other libraries

This comparison should serve as nothing more than a quick overview of
a few portable, open-source libraries for task parallelism in C++.  By
no means is this an exhaustive summary of the features those libraries
provide.

### Standard library

**C++11/14/17**

These language standards only provide a basic way of dealing with
tasks. The simplest way to launch an asynchronous task is through:
```cpp
auto future = std::async(std::launch::async, functor, param1, param2);
```
which will run the given `functor` with `param1` and `param2` on a
separate thread and return a `std::future` object. There are other
primitives such as `std::promise` and `std::packaged_task` that assist
with constructing asynchronous tasks. The latter is used internally by
transwarp to schedule functions.

Unfortunately, there is no way to chain futures together to create a
graph of dependent operations. There is also no way of _easily_
scheduling these operations on certain, user-defined threads. The
standard library does, however, provide all the tools to build a
framework, such as a transwarp, which implements these features.

**C++20 and beyond**

There are proposals through the [concurrency
ts](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html)
to extend `std::future` and `std::shared_future` to support
_continuations_, e.g.

```cpp
std::future<int> f1 = std::async([]() { return 123; });
std::future<std::string> f2 = f1.then([](std::future<int> f) { return std::to_string(f.get()); });
```

In addition, there is talk about adding support for `when_all` and
`when_any`. These features combined would make it possible to create a
dependency graph much like the one in transwarp. Future continuations
will, however, not support a re-scheduling of tasks in the graph but
rather serve as one-shot operations. Also, there seems to be currently
no efforts towards custom executors.

The above example in transwarp would look something like this:

```cpp
tw::parallel executor{4};
auto t1 = tw::make_task(tw::root, []() { return 123; });
auto t2 = tw::make_task(tw::consume, [](int x) { return std::to_string(x); }, t1);
t2->schedule_all(executor);
```

### Boost

Boost supports
[continuations](http://www.boost.org/doc/libs/1_65_1/doc/html/thread/synchronization.html#thread.synchronization.futures.then)
much like the ones proposed in the above concurrency ts. In addition,
boost supports custom executors that can be passed into overloaded
versions of `future::then` and `async`. The custom executor is
expected to implement a `submit` method accepting a `function<void()>`
which then runs the given function, possibly asynchronously. Hence,
this is quite similar to what transwarp does.

A difference to point out is that transwarp uses `std::shared_future`
to implement transfer between tasks which may be more expensive in
certain situations compared to `std::future`. Note that a call to
`get()` on a future will either return a reference or a moved result
while the same call on a shared future will either provide a reference
or a copy but never move.

Boost also supports a form of scheduling of tasks. This allows users
to schedule tasks when certain events take place, such as reaching a
certain time. In transwarp, tasks are scheduled by calling `schedule`
or `schedule_all` on the task object.

### HPX

HPX implements all of the features proposed in the concurrency ts and
currently available in boost regarding continuations. It also supports
custom executors and goes slightly beyond what boost has to
offer. This [blog
post](http://stellar-group.org/2015/07/hpx-and-cpp-futures/) has a
nice summary.

Neither Boost nor HPX seem to support task graphs for multiple
invocations.

### TBB

TBB implements its own version of [task-based
programming](https://www.threadingbuildingblocks.org/tutorial-intel-tbb-task-based-programming), for instance

```cpp
int Fib(int n) {
    if ( n < 2 ) {
        return n;
    } else {
        int x, y;
        tbb::task_group g;
        g.run([&]{ x = Fib(n-1); }); // spawn a task
        g.run([&]{ y = Fib(n-2); }); // spawn another task
        g.wait();                // wait for both tasks to complete
        return x+y;
    }
}
```

which computes the Fibonacci series in a parallel fashion. The
corresponding code in transwarp would look like this:

```cpp
tw::parallel executor{4};

int Fib(int n) {
    if ( n < 2 ) {
        return n;
    } else {
        int x, y;
        auto t1 = tw::make_task(tw::root, [&]{ x = Fib(n-1); });
        auto t2 = tw::make_task(tw::root, [&]{ y = Fib(n-2); });
        auto t3 = tw::make_task(tw::wait, []{}, t1, t2);
        t3->schedule_all(executor);
        t3->get_future().wait();
        return x+y;
    }
}
```

Note that for any real-world application the graph of tasks should be
created upfront and not on the fly.

TBB supports both automatic and fine-grained task scheduling. Creating
an acyclic graph of tasks appears to be somewhat cumbersome and is not
nearly as straightforward as in transwarp. This
[post](https://software.intel.com/en-us/node/506110) shows an example
of such a graph. Simple continuations suffer from the same usability
problem, though, it is possible to use them.

### Stlab

[Stlab](http://stlab.cc/libraries/concurrency/index.html) appears to
be the library in the list that's the closest to what transwarp is
trying to achieve. It supports future continuations in multiple
directions (essentially a graph) and also canceling futures. Stlab
splits its implementation into futures for single-shot graphs and
channels for multiple invocations. A simple example using channels:

```cpp
stlab::sender<int> send;
stlab::receiver<int> receive;

std::tie(send, receive) = stlab::channel<int>(stlab::default_executor);

std::atomic_int v{0};

auto result = receive 
    | stlab::executor{ stlab::immediate_executor } & [](int x) { return x * 2; }
    | [&v](int x) { v = x; };

receive.set_ready();

send(1);

// Waiting just for illustrational purpose
while (v == 0) {
    this_thread::sleep_for(chrono::milliseconds(1));
}
```

This will take the input provided via `send`, multiply it by two, and
then assign `v` the result. The corresponding code in transwarp would
look like this:

```cpp
int x = 0;

auto t1 = tw::make_task(tw::root, [&x] { return x; });
auto t2 = tw::make_task(tw::consume, [](int x) { return x * 2; }, t1);

x = 1;
t2->schedule_all();

int v = t2->get_future().get();
```

### Conclusions

As can be seen from the comparison, transwarp shares many similarities
to existing libraries. The notion of chaining dependent, possibly
asynchronous operations and scheduling them using custom executors is a
common use case. To summarize:

Use transwarp if:
* you want to model your dependent operations in a task graph
* you construct the task graph upfront and invoke it multiple times
* you possibly now or later want to run some tasks on different threads
* you want a header-only task library that is easy to use and has no dependencies

Don't use transwarp if:
* you construct the task graph on the fly for one-shot operations (use futures instead)
* significant chunks of memory are copied when invoking dependent tasks (transwarp uses shared_futures to communicate results between tasks)

## Feedback

Contact me if you have any questions or suggestions to make this a better library!
You can post on [gitter](https://gitter.im/bloomen/transwarp), submit a pull request,
create a Github issue, or simply email me at `chr.blume@gmail.com`.

If you're serious about contributing code to transwarp (which would be awesome!) then 
please submit a pull request and keep in mind that:
- all new development happens on the _develop_ branch while the _master_ branch is at the latest release
- unit tests should be added for all new code by extending the existing unit test suite
- C++ code uses spaces throughout 
