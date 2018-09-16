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
