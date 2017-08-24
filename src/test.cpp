#include <libunittest/all.hpp>
#include "transwarp.h"
#include "../examples/basic_with_three_tasks.h"
#include "../examples/statistical_key_facts.h"
#include "../examples/benchmark_simple.h"
#include "../examples/benchmark_statistical.h"


using transwarp::make_task;
using transwarp::make_task;


COLLECTION(test_transwarp) {

transwarp::node generic_node() {
    return {1, "cool", transwarp::task_type::consume, {}};
}

void make_test_one_task(std::size_t threads) {
    const int value = 42;
    auto f1 = [value]{ return value; };
    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task;
    if (threads > 0) {
        task = make_task(transwarp::root, f1);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task = make_task(transwarp::root, f1);
        executor = std::make_shared<transwarp::sequential>();
    }
    ASSERT_EQUAL(0u, task->get_node()->id);
    ASSERT_EQUAL(0u, task->get_node()->parents.size());
    ASSERT_EQUAL("task", task->get_node()->name);
    const auto graph = task->get_graph();
    ASSERT_EQUAL(0u, graph.size());
    task->schedule_all(*executor);
    auto future = task->get_future();
    ASSERT_EQUAL(42, future.get());
}

TEST(one_task) {
    make_test_one_task(0);
    make_test_one_task(1);
    make_test_one_task(2);
    make_test_one_task(3);
    make_test_one_task(4);
}

void make_test_three_tasks(std::size_t threads) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = make_task(transwarp::root, "t1", f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = make_task(transwarp::consume, "\nt2\t", f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task3;
    if (threads > 0) {
        task3 = make_task(transwarp::consume, "t3 ", f3, task1, task2);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task3 = make_task(transwarp::consume, "t3 ", f3, task1, task2);
        executor = std::make_shared<transwarp::sequential>();
    }

    ASSERT_EQUAL(0u, task1->get_node()->id);
    ASSERT_EQUAL(0u, task1->get_node()->parents.size());
    ASSERT_EQUAL("t1", task1->get_node()->name);

    ASSERT_EQUAL(1u, task2->get_node()->id);
    ASSERT_EQUAL(1u, task2->get_node()->parents.size());
    ASSERT_EQUAL("\nt2\t", task2->get_node()->name);

    ASSERT_EQUAL(2u, task3->get_node()->id);
    ASSERT_EQUAL(2u, task3->get_node()->parents.size());
    ASSERT_EQUAL("t3 ", task3->get_node()->name);

    task3->schedule_all(*executor);
    ASSERT_EQUAL(89, task3->get_future().get());
    ASSERT_EQUAL(42, task1->get_future().get());

    ++value;

    task3->reset_all();

    task3->schedule_all(*executor);
    ASSERT_EQUAL(91, task3->get_future().get());
    ASSERT_EQUAL(43, task1->get_future().get());

    const auto graph = task3->get_graph();
    ASSERT_EQUAL(3u, graph.size());
    const auto dot_graph = transwarp::to_string(graph);

    const std::string exp_dot_graph = "digraph {\n"
"\"t1\nroot "
"id=0 parents=0\" -> \"t2\nconsume "
"id=1 parents=1\"\n"
"\"t1\nroot "
"id=0 parents=0\" -> \"t3\nconsume "
"id=2 parents=2\"\n"
"\"t2\nconsume "
"id=1 parents=1\" -> \"t3\nconsume "
"id=2 parents=2\"\n"
"}\n";

    ASSERT_EQUAL(exp_dot_graph, dot_graph);
}

TEST(three_tasks) {
    make_test_three_tasks(0);
    make_test_three_tasks(1);
    make_test_three_tasks(2);
    make_test_three_tasks(3);
    make_test_three_tasks(4);
}

void make_test_bunch_of_tasks(std::size_t threads) {
    auto f0 = []{ return 42; };
    auto f1 = [](int a){ return 3 * a; };
    auto f2 = [](int a, int b){ return a + b; };
    auto f3 = [](int a, int b, int c){ return a + 2*b + c; };

    auto seq = std::make_shared<transwarp::sequential>();

    auto task0 = make_task(transwarp::root, f0);
    auto task1 = make_task(transwarp::root, f0);
    auto task2 = make_task(transwarp::consume, f1, task1);
    auto task3 = make_task(transwarp::consume, f2, task2, task0);
    task3->set_executor(seq);
    auto task5 = make_task(transwarp::consume, "task5", f2, task3, task2);
    auto task6 = make_task(transwarp::consume, f3, task1, task2, task5);
    auto task7 = make_task(transwarp::consume, f2, task5, task6);
    task7->set_executor(seq);
    auto task8 = make_task(transwarp::consume, f2, task6, task7);
    auto task9 = make_task(transwarp::consume, f1, task7);
    auto task10 = make_task(transwarp::consume, f1, task9);
    task10->set_executor(seq);
    auto task11 = make_task(transwarp::consume, f3, task10, task7, task8);
    auto task12 = make_task(transwarp::consume, f2, task11, task6);

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task13;
    if (threads > 0) {
        task13 = make_task(transwarp::consume, f3, task10, task11, task12);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task13 = make_task(transwarp::consume, f3, task10, task11, task12);
        executor = std::make_shared<transwarp::sequential>();
    }

    const auto task0_result = 42;
    const auto task3_result = 168;
    const auto task11_result = 11172;
    const auto exp_result = 42042;

    task13->schedule_all(*executor);
    ASSERT_EQUAL(exp_result, task13->get_future().get());
    ASSERT_EQUAL(task0_result, task0->get_future().get());
    ASSERT_EQUAL(task3_result, task3->get_future().get());
    ASSERT_EQUAL(task11_result, task11->get_future().get());

    for (auto i=0; i<100; ++i) {
        task13->schedule_all(*executor);
        ASSERT_EQUAL(task0_result, task0->get_future().get());
        ASSERT_EQUAL(task3_result, task3->get_future().get());
        ASSERT_EQUAL(task11_result, task11->get_future().get());
        ASSERT_EQUAL(exp_result, task13->get_future().get());
    }
}

TEST(bunch_of_tasks) {
    make_test_bunch_of_tasks(0);
    make_test_bunch_of_tasks(1);
    make_test_bunch_of_tasks(2);
    make_test_bunch_of_tasks(3);
    make_test_bunch_of_tasks(4);
}

TEST(transwarp_error) {
    const std::string msg = "text";
    try {
        throw transwarp::transwarp_error(msg);
    } catch (const std::runtime_error& e) {
        ASSERT_EQUAL(msg, e.what());
    }
}

TEST(task_canceled) {
    const std::string msg = "cool is canceled";
    const auto node = generic_node();
    try {
        throw transwarp::task_canceled(node);
    } catch (const transwarp::transwarp_error& e) {
        ASSERT_EQUAL(msg, e.what());
    }
}

TEST(make_dot_graph_with_empty_graph) {
    const std::vector<transwarp::edge> graph;
    const auto dot_graph = transwarp::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n}\n";
    ASSERT_EQUAL(exp_dot_graph, dot_graph);
}

TEST(make_dot_graph_with_three_nodes) {
    auto node2 = std::make_shared<transwarp::node>(transwarp::node{1, "node2", transwarp::task_type::consume, {}});
    auto node3 = std::make_shared<transwarp::node>(transwarp::node{2, "node3", transwarp::task_type::wait, {}});
    auto node1 = std::make_shared<transwarp::node>(transwarp::node{0, "node1", transwarp::task_type::consume, {node2, node3}});
    std::vector<transwarp::edge> graph;
    graph.push_back({node2, node1});
    graph.push_back({node3, node1});
    const auto dot_graph = transwarp::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n"
"\"node2\nconsume "
"id=1 parents=0\" -> \"node1\nconsume "
"id=0 parents=2\"\n"
"\"node3\nwait "
"id=2 parents=0\" -> \"node1\nconsume "
"id=0 parents=2\"\n"
"}\n";

    ASSERT_EQUAL(exp_dot_graph, dot_graph);
}

TEST(get_node) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);

    // task3
    ASSERT_EQUAL(2, task3->get_node()->id);
    ASSERT_EQUAL("task", task3->get_node()->name);
    ASSERT_EQUAL(2u, task3->get_node()->parents.size());
    ASSERT_EQUAL(task1->get_node().get(), task3->get_node()->parents[0].get());
    ASSERT_EQUAL(task2->get_node().get(), task3->get_node()->parents[1].get());

    // task1
    ASSERT_EQUAL(0, task1->get_node()->id);
    ASSERT_EQUAL("task", task1->get_node()->name);
    ASSERT_EQUAL(0u, task1->get_node()->parents.size());

    // task2
    ASSERT_EQUAL(1, task2->get_node()->id);
    ASSERT_EQUAL("task", task2->get_node()->name);
    ASSERT_EQUAL(0u, task2->get_node()->parents.size());
}

void make_test_task_with_exception_thrown(std::size_t threads) {
    auto f1 = [] {
        throw std::logic_error("from f1");
        return 42;
    };
    auto f2 = [] (int x) {
        throw std::logic_error("from f2");
        return x + 13;
    };
    auto f3 = [] (int x) {
        throw std::logic_error("from f3");
        return x + 1;
    };
    auto task1 = make_task(transwarp::root, f1);
    auto task2 = make_task(transwarp::consume, f2, task1);

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task3;
    if (threads > 0) {
        task3 = make_task(transwarp::consume, f3, task2);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task3 = make_task(transwarp::consume, f3, task2);
        executor = std::make_shared<transwarp::sequential>();
    }
    task3->schedule_all(*executor);
    try {
        task3->get_future().get();
        ASSERT_TRUE(false);
    } catch (const std::logic_error& e) {
        ASSERT_EQUAL(std::string("from f1"), e.what());
    }
}

TEST(task_with_exception_thrown) {
    make_test_task_with_exception_thrown(0);
    make_test_task_with_exception_thrown(1);
    make_test_task_with_exception_thrown(2);
    make_test_task_with_exception_thrown(3);
    make_test_task_with_exception_thrown(4);
}

template<typename Functor, typename TaskType>
void cancel_with_schedule_all(int expected, Functor functor, TaskType task_type) {
    std::atomic_bool cont(false);
    auto f0 = [&cont] {
       while (!cont);
       return 42;
    };
    auto task1 = make_task(transwarp::root, f0);
    auto task2 = make_task(task_type, functor, task1);
    transwarp::parallel executor(2);
    task2->schedule_all(executor);
    task2->cancel_all(true);
    cont = true;
    ASSERT_THROW(transwarp::task_canceled, [task2] { task2->get_future().get(); });
    task2->cancel_all(false);
    task2->reset_all();
    task2->schedule_all(executor);
    ASSERT_EQUAL(expected, task2->get_future().get());
}

TEST(cancel_with_schedule_all_called_before_in_parallel_and_uncancel) {
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, transwarp::consume);
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, transwarp::consume_any);
    cancel_with_schedule_all(13, [] () { return 13; }, transwarp::wait);
    cancel_with_schedule_all(13, [] () { return 13; }, transwarp::wait_any);
}

TEST(cancel_with_schedule_all_called_after) {
    auto f0 = [] { return 42; };
    auto f1 = [] (int x) { return x + 13; };
    auto task1 = make_task(transwarp::root, f0);
    auto task2 = make_task(transwarp::consume, f1, task1);
    task2->cancel_all(true);
    transwarp::sequential executor;
    task2->schedule_all(executor);
    ASSERT_FALSE(task2->get_future().valid());
}

TEST(itask) {
    std::shared_ptr<transwarp::itask<int>> final;
    {
        auto f0 = [] { return 42; };
        auto f1 = [] (int x) { return x + 13; };
        auto task1 = make_task(transwarp::root, f0);
        auto task2 = make_task(transwarp::consume, f1, task1);
        final = task2;
    }
    transwarp::parallel executor(2);
    final->schedule_all(executor);
    ASSERT_EQUAL(55, final->get_future().get());
}

TEST(sequenced) {
    transwarp::sequential seq;
    int value = 5;
    auto functor = [&value]{ value *= 2; };
    seq.execute(functor, std::make_shared<transwarp::node>(generic_node()));
    ASSERT_EQUAL(10, value);
}

TEST(parallel) {
    transwarp::parallel par(4);
    std::atomic_bool done(false);
    int value = 5;
    auto functor = [&value, &done]{ value *= 2; done = true; };
    par.execute(functor, std::make_shared<transwarp::node>(generic_node()));
    while (!done);
    ASSERT_EQUAL(10, value);
}

TEST(schedule_all_without_executor) {
    int x = 13;
    auto task = make_task(transwarp::root, [&x]{ x *= 2; });
    task->schedule_all();
    task->get_future().wait();
    ASSERT_EQUAL(26, x);
}

TEST(schedule_all_with_task_specific_executor) {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    task->set_executor(std::make_shared<transwarp::sequential>());
    task->schedule_all();
    ASSERT_EQUAL(84, task->get_future().get());
}

TEST(invalid_task_specific_executor) {
    auto task = make_task(transwarp::root, []{});
    auto functor = [&task] { task->set_executor(nullptr); };
    ASSERT_THROW(transwarp::transwarp_error, functor);
}

TEST(invalid_parent_task) {
    auto parent = make_task(transwarp::root, [] { return 42; });
    parent.reset();
    auto functor = [&parent] { make_task(transwarp::consume, [](int) {}, parent); };
    ASSERT_THROW(transwarp::transwarp_error, functor);
}

TEST(parallel_with_zero_threads) {
    auto functor = [] { transwarp::parallel{0}; };
    ASSERT_THROW(transwarp::detail::thread_pool_error, functor);
}

TEST(schedule_single_task) {
    int x = 13;
    auto task = make_task(transwarp::root, [&x]{ x *= 2; });
    task->schedule();
    ASSERT_EQUAL(26, x);
}

TEST(schedule_with_three_tasks_sequential) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);
    task1->schedule();
    task2->schedule();
    task3->schedule();
    ASSERT_EQUAL(55, task3->get_future().get());
    task3->schedule_all();
    ASSERT_EQUAL(55, task3->get_future().get());
}

TEST(schedule_with_three_tasks_parallel) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);
    transwarp::parallel executor{4};
    task1->schedule(executor);
    task2->schedule(executor);
    task3->schedule(executor);
    ASSERT_EQUAL(55, task3->get_future().get());
    task3->schedule_all(executor);
    ASSERT_EQUAL(55, task3->get_future().get());
}

TEST(schedule_with_three_tasks_but_different_schedule) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);
    task1->schedule();
    task3->schedule_all();
    ASSERT_EQUAL(55, task3->get_future().get());
}

TEST(schedule_with_three_tasks_wait) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = make_task(transwarp::wait, f3, task1, task2);

    ASSERT_TRUE(transwarp::task_type::root == task1->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::root == task2->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::wait == task3->get_node()->type);

    task3->schedule_all();
    ASSERT_EQUAL(17, task3->get_future().get());
}

TEST(schedule_with_three_tasks_wait_any) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = make_task(transwarp::wait_any, f3, task1, task2);

    ASSERT_TRUE(transwarp::task_type::root == task1->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::root == task2->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::wait_any == task3->get_node()->type);

    task3->schedule_all();
    ASSERT_EQUAL(17, task3->get_future().get());
}

TEST(schedule_with_three_tasks_consume_any) {
    std::atomic_bool cont(false);
    auto value1 = std::make_shared<int>(42);
    auto f1 = [&cont,value1]() -> int& {
        while (!cont);
        return *value1;
    };
    auto task1 = make_task(transwarp::root, f1);
    auto value2 = std::make_shared<int>(13);
    auto f2 = [value2]() -> int& { return *value2; }; // to test ref types
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int& x) -> int { return x; };
    auto task3 = make_task(transwarp::consume_any, f3, task1, task2);

    ASSERT_TRUE(transwarp::task_type::root == task1->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::root == task2->get_node()->type);
    ASSERT_TRUE(transwarp::task_type::consume_any == task3->get_node()->type);

    transwarp::parallel exec{4};
    task3->schedule_all(exec);
    ASSERT_EQUAL(13, task3->get_future().get());
    cont = true;
}

TEST(schedule_with_two_tasks_wait_with_void_return) {
    auto f1 = [] {};
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::wait, f2, task1);

    task2->schedule_all();
    ASSERT_EQUAL(13, task2->get_future().get());
}

TEST(schedule_with_two_tasks_wait_any_with_void_return) {
    auto f1 = [] {};
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::wait_any, f2, task1);

    task2->schedule_all();
    ASSERT_EQUAL(13, task2->get_future().get());
}

TEST(task_type_output_stream) {
    const auto os1 = transwarp::to_string(transwarp::task_type::root);
    ASSERT_EQUAL("root", os1);
    const auto os1a = transwarp::to_string(transwarp::task_type::consume);
    ASSERT_EQUAL("consume", os1a);
    const auto os1b = transwarp::to_string(transwarp::task_type::consume_any);
    ASSERT_EQUAL("consume_any", os1b);
    const auto os2 = transwarp::to_string(transwarp::task_type::wait);
    ASSERT_EQUAL("wait", os2);
    const auto os2b = transwarp::to_string(transwarp::task_type::wait_any);
    ASSERT_EQUAL("wait_any", os2b);
}

TEST(task_with_const_reference_return_type) {
    auto value = std::make_shared<const int>(42);
    auto functor = [value]() -> const int& { return *value; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    ASSERT_EQUAL(*value, task->get_future().get());
}

TEST(task_with_reference_return_type) {
    auto value = std::make_shared<int>(42);
    auto functor = [value]() -> int& { return *value; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    ASSERT_EQUAL(*value, task->get_future().get());
}

COLLECTION(test_examples) {

TEST(basic_with_three_tasks) {
    std::ostringstream os;
    examples::basic_with_three_tasks(os);
    const std::string expected = "result = 55.3\nresult = 58.8\n";
    ASSERT_EQUAL(expected, os.str());
}

void make_test_statistical_keys_facts(bool parallel) {
    std::ostringstream os;
    examples::statistical_key_facts(os, 10000, parallel);
    ASSERT_GREATER(os.str().size(), 0u);
}

TEST(statistical_key_facts) {
    make_test_statistical_keys_facts(false);
    make_test_statistical_keys_facts(true);
}

TEST(benchmark_simple) {
    std::ostringstream os;
    examples::benchmark_simple(os, 10);
    ASSERT_GREATER(os.str().size(), 0u);
}

TEST(benchmark_statistical) {
    std::ostringstream os;
    examples::benchmark_statistical(os, 3);
    ASSERT_GREATER(os.str().size(), 0u);
}

} // test_examples
} // test_transwarp
