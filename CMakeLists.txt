cmake_minimum_required(VERSION 3.0)
project(transwarp)

option(transwarp_build_tests "Whether to build the transwarp tests" OFF)
option(transwarp_use_cpp11 "Whether transwarp uses C++11 to build tests. Defaults to C++17" OFF)
option(transwarp_enable_asan "Build transwarp tests with address sanitizer." OFF)
option(transwarp_enable_tsan "Build transwarp tests with thread sanitizer." OFF)

if(transwarp_enable_asan AND transwarp_enable_tsan)
    message(FATAL_ERROR "transwarp_enable_asan and transwarp_enable_tsan cannot both be ON")
endif()

install(FILES include/transwarp.h DESTINATION include)

if (transwarp_build_tests)

    enable_testing()

    if (transwarp_use_cpp11)
        set(CMAKE_CXX_STANDARD 11)
        add_compile_definitions(TRANSWARP_CPP11)
    else()
        set(CMAKE_CXX_STANDARD 17)
    endif()
    set(CMAKE_CXX_EXTENSIONS OFF)

    if (MSVC)
        add_compile_options(/W4 /bigobj /EHsc /wd4503 /wd4996 /wd4702 /wd4100)
        if(${MSVC_VERSION} GREATER_EQUAL 1929)
            if(transwarp_enable_asan)
                add_compile_options(/fsanitize=address)
            endif()
        endif()
    else()
        add_compile_options(-Wpedantic -Wall -Wextra -Wconversion -Wno-parentheses)
        if (CMAKE_COMPILER_IS_GNUCC)
            add_compile_options(-pthread)
        endif()
        if(transwarp_enable_asan)
            add_compile_options(-fsanitize=address)
            add_link_options(-fsanitize=address)
        endif()
        if(transwarp_enable_tsan)
            add_compile_options(-fsanitize=thread)
            add_link_options(-fsanitize=thread)
        endif()
    endif()

    include_directories(include)

    set(TRANSWARP_TEST_SOURCES
        test/catch_amalgamated.hpp
        test/catch_amalgamated.cpp
        test/test.h
        test/test.cpp
        test/test_accept_type.cpp
        test/test_cancel.cpp
        test/test_circular_buffer.cpp
        test/test_clone.cpp
        test/test_consume_type.cpp
        test/test_events.cpp
        test/test_examples.cpp
        test/test_exceptions.cpp
        test/test_executors.cpp
        test/test_for_each.cpp
        test/test_task_count.cpp
        test/test_task_pool.cpp
        test/test_timer.cpp
        test/test_make_task.cpp
        test/test_next.cpp
        test/test_reset.cpp
        test/test_schedule.cpp
        test/test_to_string.cpp
        test/test_transform.cpp
        test/test_value_task.cpp
        test/test_vector_parents.cpp
        test/test_wait_type.cpp
        examples/basic_with_three_tasks.cpp
        examples/benchmark_simple.cpp
        examples/benchmark_statistical.cpp
        examples/single_thread_lock_free.cpp
        examples/statistical_key_facts.cpp
        examples/wide_graph_with_pool.cpp)

    add_executable(transwarp_test ${TRANSWARP_TEST_SOURCES})
    add_executable(transwarp_minsize_test ${TRANSWARP_TEST_SOURCES})

    target_compile_definitions(transwarp_test PRIVATE UNITTEST)
    target_compile_definitions(transwarp_minsize_test PRIVATE UNITTEST TRANSWARP_MINIMUM_TASK_SIZE)

    add_executable(basic_with_three_tasks
        examples/basic_with_three_tasks.h
        examples/basic_with_three_tasks.cpp)

    add_executable(benchmark_simple
        examples/benchmark_simple.h
        examples/benchmark_simple.cpp)

    add_executable(benchmark_statistical
        examples/benchmark_statistical.h
        examples/benchmark_statistical.cpp)

    add_executable(single_thread_lock_free
        examples/single_thread_lock_free.h
        examples/single_thread_lock_free.cpp)

    add_executable(statistical_key_facts
        examples/statistical_key_facts.h
        examples/statistical_key_facts.cpp)

    add_executable(wide_graph_with_pool
        examples/wide_graph_with_pool.h
        examples/wide_graph_with_pool.cpp)

    add_executable(minimum_task_size
        examples/minimum_task_size.cpp)

    if (MSVC)
    else()
        target_link_libraries(transwarp_test -pthread)
        target_link_libraries(transwarp_minsize_test -pthread)
        target_link_libraries(basic_with_three_tasks -pthread)
        target_link_libraries(benchmark_simple -pthread)
        target_link_libraries(benchmark_statistical -pthread)
        target_link_libraries(single_thread_lock_free -pthread)
        target_link_libraries(statistical_key_facts -pthread)
        target_link_libraries(wide_graph_with_pool -pthread)
        target_link_libraries(minimum_task_size -pthread)
    endif()

    add_test(transwarp_test transwarp_test)
    add_test(transwarp_minsize_test transwarp_minsize_test)
    add_test(basic_with_three_tasks basic_with_three_tasks)
    add_test(benchmark_simple benchmark_simple)
    add_test(benchmark_statistical benchmark_statistical)
    add_test(single_thread_lock_free single_thread_lock_free)
    add_test(statistical_key_facts statistical_key_facts)
    add_test(wide_graph_with_pool wide_graph_with_pool)
    add_test(minimum_task_size minimum_task_size)

endif()
