cmake_minimum_required(VERSION 2.8)
project(transwarp)
 
include_directories(examples)
include_directories(src)

set(SOURCES src/test.cpp examples/basic_with_three_tasks.cpp  examples/single_thread_lock_free.cpp examples/benchmark_simple.cpp examples/statistical_key_facts.cpp examples/benchmark_statistical.cpp)
 
add_executable(transwarp_test ${SOURCES})
add_executable(basic_with_three_tasks examples/basic_with_three_tasks.cpp)
add_executable(single_thread_lock_free examples/single_thread_lock_free.cpp)
add_executable(benchmark_simple examples/benchmark_simple.cpp)
add_executable(statistical_key_facts examples/statistical_key_facts.cpp)
add_executable(benchmark_statistical examples/benchmark_statistical.cpp)

target_compile_definitions(transwarp_test PRIVATE UNITTEST)

if (MSVC)
   add_compile_options(/W4)
else()
	ADD_DEFINITIONS(
    	-std=c++11
	)    
	add_compile_options(-pedantic -Wall -Wextra -Wconversion -Werror)
	target_link_libraries(transwarp_test pthread)
	target_link_libraries(basic_with_three_tasks pthread)
	target_link_libraries(single_thread_lock_free pthread)
	target_link_libraries(benchmark_simple pthread)
	target_link_libraries(statistical_key_facts pthread)
	target_link_libraries(benchmark_statistical pthread)
endif()
