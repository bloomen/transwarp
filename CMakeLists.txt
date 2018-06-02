cmake_minimum_required(VERSION 2.8)
project(transwarp)
enable_testing()
 
include_directories(examples)
 
find_package(Threads)

add_executable(transwarp_test
	src/transwarp.h 
	src/test.cpp 
	examples/basic_with_three_tasks.cpp  
	examples/single_thread_lock_free.cpp 
	examples/benchmark_simple.cpp 
	examples/statistical_key_facts.cpp 
	examples/benchmark_statistical.cpp
	examples/wide_graph_with_listener.cpp)

add_executable(basic_with_three_tasks 
	src/transwarp.h 
	examples/basic_with_three_tasks.h
	examples/basic_with_three_tasks.cpp)

add_executable(single_thread_lock_free 
	src/transwarp.h 
	examples/single_thread_lock_free.h
	examples/single_thread_lock_free.cpp)

add_executable(benchmark_simple 
	src/transwarp.h 
	examples/benchmark_simple.h
	examples/benchmark_simple.cpp)

add_executable(statistical_key_facts 
	src/transwarp.h 
	examples/statistical_key_facts.h
	examples/statistical_key_facts.cpp)

add_executable(benchmark_statistical 
	src/transwarp.h 
	examples/benchmark_statistical.h
	examples/benchmark_statistical.cpp)

add_executable(wide_graph_with_listener 
	src/transwarp.h 
	examples/wide_graph_with_listener.h
	examples/wide_graph_with_listener.cpp)

target_compile_definitions(transwarp_test PRIVATE UNITTEST)

add_test(transwarp_test transwarp_test --use-colour no)
add_test(basic_with_three_tasks basic_with_three_tasks)
add_test(single_thread_lock_free single_thread_lock_free)
add_test(benchmark_simple benchmark_simple)
add_test(statistical_key_facts statistical_key_facts)
add_test(benchmark_statistical benchmark_statistical)
add_test(wide_graph_with_listener wide_graph_with_listener)

if (MSVC)
   set(CMAKE_CXX_FLAGS "/W4 /bigobj /EHsc /wd4503 /wd4996 /wd4702")
else()
   set(CMAKE_CXX_FLAGS "-std=c++11 -pedantic -Wall -Wextra -Wconversion")
   if (CMAKE_COMPILER_IS_GNUCC)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
   endif()
   target_link_libraries(transwarp_test ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(basic_with_three_tasks ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(single_thread_lock_free ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(benchmark_simple ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(statistical_key_facts ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(benchmark_statistical ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(wide_graph_with_listener ${CMAKE_THREAD_LIBS_INIT})
endif()
