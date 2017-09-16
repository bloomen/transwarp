cmake_minimum_required(VERSION 2.8)
project(transwarp)
 
include_directories(examples)
include_directories(src)

set(SOURCES src/test.cpp examples/basic_with_three_tasks.cpp  examples/single_thread_lock_free.cpp examples/benchmark_simple.cpp examples/statistical_key_facts.cpp examples/benchmark_statistical.cpp)

find_package(Threads)

add_executable(transwarp_test ${SOURCES})
add_executable(basic_with_three_tasks examples/basic_with_three_tasks.cpp)
add_executable(single_thread_lock_free examples/single_thread_lock_free.cpp)
add_executable(benchmark_simple examples/benchmark_simple.cpp)
add_executable(statistical_key_facts examples/statistical_key_facts.cpp)
add_executable(benchmark_statistical examples/benchmark_statistical.cpp)

target_compile_definitions(transwarp_test PRIVATE UNITTEST)

if (MSVC)
   set(CMAKE_CXX_FLAGS "/W4 /bigobj /wd4503")
else()
   set(CMAKE_CXX_FLAGS "-std=c++11 -pedantic -Wall -Wextra -Wconversion -Werror -pthread")
   target_link_libraries(transwarp_test ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(basic_with_three_tasks ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(single_thread_lock_free ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(benchmark_simple ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(statistical_key_facts ${CMAKE_THREAD_LIBS_INIT})
   target_link_libraries(benchmark_statistical ${CMAKE_THREAD_LIBS_INIT})
endif()
