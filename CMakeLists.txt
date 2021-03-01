cmake_minimum_required(VERSION 2.8)
project(transwarp)
enable_testing()

set(USE_CPP11 0 CACHE STRING "")

include_directories(examples)

find_package(Threads)

set(SOURCES
	src/transwarp.h
	src/test.h
	src/test.cpp
	src/test_accept_type.cpp
	src/test_cancel.cpp
	src/test_circular_buffer.cpp
	src/test_clone.cpp
	src/test_consume_type.cpp
	src/test_events.cpp
	src/test_examples.cpp
	src/test_exceptions.cpp
	src/test_executors.cpp
	src/test_for_each.cpp
	src/test_task_count.cpp
	src/test_task_pool.cpp
	src/test_timer.cpp
	src/test_make_task.cpp
	src/test_next.cpp
	src/test_reset.cpp
	src/test_schedule.cpp
	src/test_to_string.cpp
	src/test_transform.cpp
	src/test_value_task.cpp
	src/test_vector_parents.cpp
	src/test_wait_type.cpp
	examples/basic_with_three_tasks.cpp
	examples/benchmark_simple.cpp
	examples/benchmark_statistical.cpp
	examples/single_thread_lock_free.cpp
	examples/statistical_key_facts.cpp
	examples/wide_graph_with_pool.cpp)

add_executable(transwarp_test ${SOURCES})
add_executable(transwarp_minsize_test ${SOURCES})

add_executable(basic_with_three_tasks
	src/transwarp.h
	examples/basic_with_three_tasks.h
	examples/basic_with_three_tasks.cpp)

add_executable(benchmark_simple
	src/transwarp.h
	examples/benchmark_simple.h
	examples/benchmark_simple.cpp)

add_executable(benchmark_statistical
	src/transwarp.h
	examples/benchmark_statistical.h
	examples/benchmark_statistical.cpp)

add_executable(single_thread_lock_free
	src/transwarp.h
	examples/single_thread_lock_free.h
	examples/single_thread_lock_free.cpp)

add_executable(statistical_key_facts
	src/transwarp.h
	examples/statistical_key_facts.h
	examples/statistical_key_facts.cpp)

add_executable(wide_graph_with_pool
	src/transwarp.h
	examples/wide_graph_with_pool.h
	examples/wide_graph_with_pool.cpp)

add_executable(minimum_task_size
	src/transwarp.h
	examples/minimum_task_size.cpp)

target_compile_definitions(transwarp_test PRIVATE UNITTEST $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(transwarp_minsize_test PRIVATE UNITTEST TRANSWARP_MINIMUM_TASK_SIZE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(basic_with_three_tasks PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(benchmark_simple PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(benchmark_statistical PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(single_thread_lock_free PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(statistical_key_facts PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(wide_graph_with_pool PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)
target_compile_definitions(minimum_task_size PRIVATE $<${USE_CPP11}:TRANSWARP_CPP11>)

add_test(transwarp_test transwarp_test --use-colour no)
add_test(transwarp_minsize_test transwarp_minsize_test --use-colour no)
add_test(basic_with_three_tasks basic_with_three_tasks)
add_test(benchmark_simple benchmark_simple)
add_test(benchmark_statistical benchmark_statistical)
add_test(single_thread_lock_free single_thread_lock_free)
add_test(statistical_key_facts statistical_key_facts)
add_test(wide_graph_with_pool wide_graph_with_pool)
add_test(minimum_task_size minimum_task_size)

if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /bigobj /EHsc /wd4503 /wd4996 /wd4702 /wd4100")
	if (USE_CPP11)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
	endif()
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra -Wconversion")
	if (USE_CPP11)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif()
	if (CMAKE_COMPILER_IS_GNUCC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
	endif()
	target_link_libraries(transwarp_test ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(transwarp_minsize_test ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(basic_with_three_tasks ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(benchmark_simple ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(benchmark_statistical ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(single_thread_lock_free ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(statistical_key_facts ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(wide_graph_with_pool ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(minimum_task_size ${CMAKE_THREAD_LIBS_INIT})
endif()
