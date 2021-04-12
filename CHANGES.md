2.2.2

- update cmake and directory structure
- remove deprecated use of std::result_of

2.2.1

- added support for C++11 via TRANSWARP_CPP11

2.2.0

- added compile time switches to control the task size
- added the `releaser` listener to allow releasing intermediate results
- added a callback to the parallel executor to do something on thread start (like thread naming)
- improved documentation and tests

2.1.0

- added new events: `after_future_changed`, `after_custom_data_set`
- made members of `functor` and `edge` mutable and added corresponding accessors
- removed `const` from the task parameter of the `listener` and `executor` interfaces
- refactored common stuff between `task_impl` and `value_task` into a new base class 

2.0.0

- breaking changes!
- switched to C++17 (retained `transwarp1.X` branch for C++11 support)
- removed `transwarp::node` and merged its members into the task classes
- renamed some methods and changed method signatures
- now using `std::optional` and `std::any`
- added ability to clone tasks and used that in the `task_pool`

1.9.0

- added `for_each` and `transform` free functions
- added `get_task_count` and `get_parent_count` methods to the task class
- performance optimizations regarding scheduling tasks
- split up the test suite into multiple files
- improved documentation

1.8.1

- timer class: Added tracking of idle time along with wait and run time

1.8.0

- added a timer that can be added as a listener (node will carry the timing info)
- added methods to add/remove listeners to/from all tasks in the graph
- improved documentation

1.7.0

- added a graph pool to run different instances of the same graph in parallel

1.6.1

- various code and performance improvements
- improved documentation

1.6.0

- added support for parents provided as a std::vector of tasks
- ensured that abandoned tasks are canceled
- added new events: before_invoked and after_canceled
- fixed bug where tasks were still canceled after reset

1.5.0

- added a 'then()' method to chain tasks when a child only has one parent
- added more useful functions to add/remove listeners
- corrected the naming of the events
- ensured that a value task can store volatile types

1.4.0

- added a schedule type enum and made 'breadth' the default
- added a listener interface to handle task events
- added a new example that demonstrates scheduling a wide graph
- improved the performance of scheduling a graph
- fixed up documentation, added doxygen doc

1.3.0

- added new task types: accept and accept_any
- added new functions: set_value and set_exception
- added a value_task class that doesn't require scheduling
- improved error reporting 
- general code clean up; addressed compiler warnings
- simplify the node class by addressing the constructors

1.2.1

- added an explicit check to certain methods to ensure that a task was scheduled 

1.2.0

- added means of canceling a task while its running through transwarp::functor
- added method 'was_scheduled()' to the task class to check if the task was scheduled
- added method 'is_ready()' to check if a result is available
- added new methods to the task class that forward to all parent tasks
- added safety checks to methods that shouldn't be called when a task is running

1.1.0

- added 'wait()' method to wait for the task to finish
- added 'get()' method to retrieve the task result
- added methods to assign/query priority and custom data of a task
- improved documentation

1.0.1

- improved documentation
- added gitter badge
- added script for checking all of transwarp

1.0.0

- made the library safer and more user-friendly
- merge final_task and task together to a single class
- add task types to support consuming and waiting
- added ability to assign custom executors per task
- changed schedule functions to accept an executor
- improved documentation and examples
- switch to using cmake and catch
- add continuous integration on Linux, Mac, and Windows

0.2.0

- added ability to use custom executors
- added ability to specify a priority for tasks and their execution
- added noexcept keyword where possible
- removed ability to pause execution
- better documentation

0.1.0

- initial release
