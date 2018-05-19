#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "transwarp.h"
#include "../examples/basic_with_three_tasks.h"
#include "../examples/statistical_key_facts.h"
#include "../examples/benchmark_simple.h"
#include "../examples/benchmark_statistical.h"
#include "../examples/single_thread_lock_free.h"
#include <array>


using transwarp::make_task;
using transwarp::make_value_task;

using nodes_t = std::vector<std::shared_ptr<transwarp::node>>;


std::shared_ptr<transwarp::node> generic_node() {
    auto node = std::make_shared<transwarp::node>();
    transwarp::detail::node_manip::set_type(*node, transwarp::task_type::consume);
    transwarp::detail::node_manip::set_name(*node, std::make_shared<std::string>("cool"));
    transwarp::detail::node_manip::set_id(*node, 1);
    return node;
}

void make_test_one_task(std::size_t threads, transwarp::schedule_type type) {
    const int value = 42;
    auto f1 = [value]{ return value; };
    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::task<int>> task;
    if (threads > 0) {
        task = make_task(transwarp::root, f1);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task = make_task(transwarp::root, f1);
        executor = std::make_shared<transwarp::sequential>();
    }
    REQUIRE(0u == task->get_node()->get_id());
    REQUIRE(0u == task->get_node()->get_parents().size());
    REQUIRE_FALSE(task->get_node()->get_name());
    const auto graph = task->get_graph();
    REQUIRE(0u == graph.size());
    task->schedule_all(*executor, type);
    auto future = task->get_future();
    REQUIRE(42 == future.get());
}

TEST_CASE("one_task_with_schedule_by_depth") {
    auto type = transwarp::schedule_type::depth;
    make_test_one_task(0, type);
    make_test_one_task(1, type);
    make_test_one_task(2, type);
    make_test_one_task(3, type);
    make_test_one_task(4, type);
}

TEST_CASE("one_task_with_schedule_by_breadth") {
    auto type = transwarp::schedule_type::breadth;
    make_test_one_task(0, type);
    make_test_one_task(1, type);
    make_test_one_task(2, type);
    make_test_one_task(3, type);
    make_test_one_task(4, type);
}

void make_test_three_tasks(std::size_t threads, transwarp::schedule_type type) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = make_task(transwarp::root, "t1", f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = make_task(transwarp::consume, "t2", f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::task<int>> task3;
    if (threads > 0) {
        task3 = make_task(transwarp::consume, "t3", f3, task1, task2);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task3 = make_task(transwarp::consume, "t3", f3, task1, task2);
        executor = std::make_shared<transwarp::sequential>();
    }

    REQUIRE(0u == task1->get_node()->get_id());
    REQUIRE(0u == task1->get_node()->get_parents().size());
    REQUIRE("t1" == *task1->get_node()->get_name());

    REQUIRE(1u == task2->get_node()->get_id());
    REQUIRE(1u == task2->get_node()->get_parents().size());
    REQUIRE("t2" == *task2->get_node()->get_name());
    task2->set_executor(std::make_shared<transwarp::sequential>());

    REQUIRE(2u == task3->get_node()->get_id());
    REQUIRE(2u == task3->get_node()->get_parents().size());
    REQUIRE("t3" == *task3->get_node()->get_name());

    REQUIRE_FALSE(task1->was_scheduled());
    REQUIRE_FALSE(task2->was_scheduled());
    REQUIRE_FALSE(task3->was_scheduled());

    REQUIRE_THROWS_AS(task1->is_ready(), transwarp::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task2->is_ready(), transwarp::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task3->is_ready(), transwarp::transwarp_error); // not scheduled yet

    task3->schedule_all(*executor, type);

    REQUIRE(task1->was_scheduled());
    REQUIRE(task2->was_scheduled());
    REQUIRE(task3->was_scheduled());

    REQUIRE(89 == task3->get_future().get());
    REQUIRE(42 == task1->get_future().get());

    REQUIRE(task1->is_ready());
    REQUIRE(task2->is_ready());
    REQUIRE(task3->is_ready());

    ++value;

    task3->schedule_all(*executor, type);
    REQUIRE(91 == task3->get_future().get());
    REQUIRE(43 == task1->get_future().get());

    const auto graph = task3->get_graph();
    REQUIRE(3u == graph.size());
    const auto dot_graph = transwarp::to_string(graph);

    const std::string exp_dot_graph = "digraph {\n"
"\"<t1>\nroot "
"id=0 par=0\" -> \"<t2>\nconsume "
"id=1 par=1\n<transwarp::sequential>\"\n"
"\"<t1>\nroot "
"id=0 par=0\" -> \"<t3>\nconsume "
"id=2 par=2\"\n"
"\"<t2>\nconsume "
"id=1 par=1\n<transwarp::sequential>\" -> \"<t3>\nconsume "
"id=2 par=2\"\n"
"}";

    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("three_tasks_width_schedule_by_depth") {
    auto type = transwarp::schedule_type::depth;
    make_test_three_tasks(0, type);
    make_test_three_tasks(1, type);
    make_test_three_tasks(2, type);
    make_test_three_tasks(3, type);
    make_test_three_tasks(4, type);
}

TEST_CASE("three_tasks_width_schedule_by_breadth") {
    auto type = transwarp::schedule_type::breadth;
    make_test_three_tasks(0, type);
    make_test_three_tasks(1, type);
    make_test_three_tasks(2, type);
    make_test_three_tasks(3, type);
    make_test_three_tasks(4, type);
}

void make_test_bunch_of_tasks(std::size_t threads, transwarp::schedule_type type) {
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
    std::shared_ptr<transwarp::task<int>> task13;
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

    task13->schedule_all(*executor, type);
    REQUIRE(exp_result == task13->get_future().get());
    REQUIRE(task0_result == task0->get_future().get());
    REQUIRE(task3_result == task3->get_future().get());
    REQUIRE(task11_result == task11->get_future().get());

    for (auto i=0; i<100; ++i) {
        task13->schedule_all(*executor, type);
        REQUIRE(task0_result == task0->get_future().get());
        REQUIRE(task3_result == task3->get_future().get());
        REQUIRE(task11_result == task11->get_future().get());
        REQUIRE(exp_result == task13->get_future().get());
    }
}

TEST_CASE("bunch_of_tasks_with_schedule_by_depth") {
    auto type = transwarp::schedule_type::depth;
    make_test_bunch_of_tasks(0, type);
    make_test_bunch_of_tasks(1, type);
    make_test_bunch_of_tasks(2, type);
    make_test_bunch_of_tasks(3, type);
    make_test_bunch_of_tasks(4, type);
}

TEST_CASE("bunch_of_tasks_with_schedule_by_breadth") {
    auto type = transwarp::schedule_type::breadth;
    make_test_bunch_of_tasks(0, type);
    make_test_bunch_of_tasks(1, type);
    make_test_bunch_of_tasks(2, type);
    make_test_bunch_of_tasks(3, type);
    make_test_bunch_of_tasks(4, type);
}

TEST_CASE("transwarp_error") {
    const std::string msg = "text";
    try {
        throw transwarp::transwarp_error(msg);
    } catch (const std::runtime_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("task_canceled") {
    const std::string msg = "task canceled: node";
    try {
        throw transwarp::task_canceled("node");
    } catch (const transwarp::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("task_destroyed") {
    const std::string msg = "task destroyed: node";
    try {
        throw transwarp::task_destroyed("node");
    } catch (const transwarp::transwarp_error& e) {
        REQUIRE(msg == e.what());
    }
}

TEST_CASE("make_dot_graph_with_empty_graph") {
    const std::vector<transwarp::edge> graph;
    const auto dot_graph = transwarp::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n}";
    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("make_dot_graph_with_three_nodes") {
    auto node2 = std::make_shared<transwarp::node>();
    transwarp::detail::node_manip::set_type(*node2, transwarp::task_type::consume);
    transwarp::detail::node_manip::set_name(*node2, std::make_shared<std::string>("node2"));
    transwarp::detail::node_manip::set_id(*node2, 1);
    auto node3 = std::make_shared<transwarp::node>();
    transwarp::detail::node_manip::set_type(*node3, transwarp::task_type::wait);
    transwarp::detail::node_manip::set_name(*node3, std::make_shared<std::string>("node3"));
    transwarp::detail::node_manip::set_id(*node3, 2);
    transwarp::detail::node_manip::set_executor(*node3, std::make_shared<std::string>("exec"));
    auto node1 = std::make_shared<transwarp::node>();
    transwarp::detail::node_manip::set_type(*node1, transwarp::task_type::consume);
    transwarp::detail::node_manip::set_name(*node1, std::make_shared<std::string>("node1"));
    transwarp::detail::node_manip::set_id(*node1, 0);
    transwarp::detail::node_manip::add_parent(*node1, node2);
    transwarp::detail::node_manip::add_parent(*node1, node3);
    std::vector<transwarp::edge> graph;
    graph.emplace_back(node2, node1);
    graph.emplace_back(node3, node1);
    const auto dot_graph = transwarp::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n"
"\"<node2>\nconsume "
"id=1 par=0\" -> \"<node1>\nconsume "
"id=0 par=2\"\n"
"\"<node3>\nwait "
"id=2 par=0\n<exec>\" -> \"<node1>\nconsume "
"id=0 par=2\"\n"
"}";

    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("get_node") {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);

    // task3
    REQUIRE(2 == task3->get_node()->get_id());
    REQUIRE_FALSE(task3->get_node()->get_name());
    REQUIRE(2u == task3->get_node()->get_parents().size());
    REQUIRE(task1->get_node().get() == task3->get_node()->get_parents()[0].get());
    REQUIRE(task2->get_node().get() == task3->get_node()->get_parents()[1].get());

    // task1
    REQUIRE(0 == task1->get_node()->get_id());
    REQUIRE_FALSE(task1->get_node()->get_name());
    REQUIRE(0u == task1->get_node()->get_parents().size());

    // task2
    REQUIRE(1 == task2->get_node()->get_id());
    REQUIRE_FALSE(task2->get_node()->get_name());
    REQUIRE(0u == task2->get_node()->get_parents().size());
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
    std::shared_ptr<transwarp::task<int>> task3;
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
        REQUIRE(false);
    } catch (const std::logic_error& e) {
        REQUIRE(std::string("from f1") == e.what());
    }
}

TEST_CASE("task_with_exception_thrown") {
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
    REQUIRE_THROWS_AS(task2->get_future().get(), transwarp::task_canceled);
    task2->cancel_all(false);
    task2->schedule_all(executor);
    REQUIRE(expected == task2->get_future().get());
}

TEST_CASE("cancel_with_schedule_all_called_before_in_parallel_and_uncancel") {
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, transwarp::consume);
    cancel_with_schedule_all(55, [] (int x) { return x + 13; }, transwarp::consume_any);
    cancel_with_schedule_all(13, [] () { return 13; }, transwarp::wait);
    cancel_with_schedule_all(13, [] () { return 13; }, transwarp::wait_any);
}

TEST_CASE("cancel_with_schedule_all_called_after") {
    auto f0 = [] { return 42; };
    auto f1 = [] (int x) { return x + 13; };
    auto task1 = make_task(transwarp::root, f0);
    auto task2 = make_task(transwarp::consume, f1, task1);
    task2->cancel_all(true);
    transwarp::sequential executor;
    task2->schedule_all(executor);
    REQUIRE_FALSE(task2->get_future().valid());
}

TEST_CASE("itask") {
    std::shared_ptr<transwarp::task<int>> final;
    {
        auto f0 = [] { return 42; };
        auto f1 = [] (int x) { return x + 13; };
        auto task1 = make_task(transwarp::root, f0);
        auto task2 = make_task(transwarp::consume, f1, task1);
        final = task2;
    }
    transwarp::parallel executor(2);
    final->schedule_all(executor);
    REQUIRE(55 == final->get_future().get());
}

TEST_CASE("sequenced") {
    transwarp::sequential seq;
    int value = 5;
    auto functor = [&value]{ value *= 2; };
    seq.execute(functor, generic_node());
    REQUIRE(10 == value);
}

TEST_CASE("parallel") {
    transwarp::parallel par(4);
    std::atomic_bool done(false);
    int value = 5;
    auto functor = [&value, &done]{ value *= 2; done = true; };
    par.execute(functor, generic_node());
    while (!done);
    REQUIRE(10 == value);
}

TEST_CASE("schedule_all_without_executor") {
    int x = 13;
    auto task = make_task(transwarp::root, [&x]{ x *= 2; });
    task->schedule_all();
    task->get_future().wait();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_all_without_executor_wait_method") {
    int x = 13;
    auto task = make_task(transwarp::root, [&x]{ x *= 2; });
    REQUIRE_THROWS_AS(task->wait(), transwarp::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task->get(), transwarp::transwarp_error); // not scheduled yet
    task->schedule_all();
    task->wait();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_all_with_task_specific_executor") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    task->set_executor(std::make_shared<transwarp::sequential>());
    task->schedule_all();
    REQUIRE(84 == task->get());
}

TEST_CASE("invalid_task_specific_executor") {
    auto task = make_task(transwarp::root, []{});
    REQUIRE_THROWS_AS(task->set_executor(nullptr), transwarp::transwarp_error);
}

TEST_CASE("invalid_parent_task") {
    auto parent = make_task(transwarp::root, [] { return 42; });
    parent.reset();
    REQUIRE_THROWS_AS(make_task(transwarp::consume, [](int) {}, parent), transwarp::transwarp_error);
}

TEST_CASE("parallel_with_zero_threads") {
    REQUIRE_THROWS_AS(transwarp::parallel{0}, transwarp::thread_pool_error);
}

TEST_CASE("schedule_single_task") {
    int x = 13;
    auto task = make_task(transwarp::root, [&x]{ x *= 2; });
    task->schedule();
    REQUIRE(26 == x);
}

TEST_CASE("schedule_with_three_tasks_sequential") {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);
    task1->schedule();
    task2->schedule();
    task3->schedule();
    REQUIRE(55 == task3->get());
    task3->schedule_all();
    REQUIRE(55 == task3->get());
}

TEST_CASE("schedule_with_three_tasks_parallel") {
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
    REQUIRE(55 == task3->get_future().get());
    task3->schedule_all(executor);
    REQUIRE(55 == task3->get_future().get());
}

TEST_CASE("schedule_with_three_tasks_but_different_schedule") {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(transwarp::consume, f3, task1, task2);
    task1->schedule();
    task3->schedule_all();
    REQUIRE(55 == task3->get_future().get());
}

TEST_CASE("schedule_with_three_tasks_wait") {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = make_task(transwarp::wait, f3, task1, task2);

    REQUIRE(transwarp::task_type::root == task1->get_node()->get_type());
    REQUIRE(transwarp::task_type::root == task2->get_node()->get_type());
    REQUIRE(transwarp::task_type::wait == task3->get_node()->get_type());

    task3->schedule_all();
    REQUIRE(17 == task3->get_future().get());
}

TEST_CASE("schedule_with_three_tasks_wait_any") {
    auto f1 = [] { return 42; };
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::root, f2);
    auto f3 = []() { return 17; };
    auto task3 = make_task(transwarp::wait_any, f3, task1, task2);

    REQUIRE(transwarp::task_type::root == task1->get_node()->get_type());
    REQUIRE(transwarp::task_type::root == task2->get_node()->get_type());
    REQUIRE(transwarp::task_type::wait_any == task3->get_node()->get_type());

    task3->schedule_all();
    REQUIRE(17 == task3->get_future().get());
}

TEST_CASE("schedule_with_three_tasks_consume_any") {
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

    REQUIRE(transwarp::task_type::root == task1->get_node()->get_type());
    REQUIRE(transwarp::task_type::root == task2->get_node()->get_type());
    REQUIRE(transwarp::task_type::consume_any == task3->get_node()->get_type());

    transwarp::parallel exec{4};
    task3->schedule_all(exec);
    REQUIRE(13 == task3->get_future().get());
    cont = true;
}

TEST_CASE("schedule_with_two_tasks_wait_with_void_return") {
    auto f1 = [] {};
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::wait, f2, task1);

    task2->schedule_all();
    REQUIRE(13 == task2->get_future().get());
}

TEST_CASE("schedule_with_two_tasks_wait_any_with_void_return") {
    auto f1 = [] {};
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::wait_any, f2, task1);

    task2->schedule_all();
    REQUIRE(13 == task2->get_future().get());
}

TEST_CASE("task_type_to_string") {
    REQUIRE("root" == transwarp::to_string(transwarp::task_type::root));
    REQUIRE("accept" == transwarp::to_string(transwarp::task_type::accept));
    REQUIRE("accept_any" == transwarp::to_string(transwarp::task_type::accept_any));
    REQUIRE("consume" == transwarp::to_string(transwarp::task_type::consume));
    REQUIRE("consume_any" == transwarp::to_string(transwarp::task_type::consume_any));
    REQUIRE("wait" == transwarp::to_string(transwarp::task_type::wait));
    REQUIRE("wait_any" == transwarp::to_string(transwarp::task_type::wait_any));
}

TEST_CASE("task_with_const_reference_return_type") {
    auto value = std::make_shared<const int>(42);
    auto functor = [value]() -> const int& { return *value; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    REQUIRE(*value == task->get_future().get());
    REQUIRE(*value == task->get());
}

TEST_CASE("task_with_reference_return_type") {
    auto value = std::make_shared<int>(42);
    auto functor = [value]() -> int& { return *value; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    REQUIRE(*value == task->get_future().get());
    REQUIRE(*value == task->get());
}

struct non_copy_functor {
    non_copy_functor() = default;
    non_copy_functor(const non_copy_functor&) = delete;
    non_copy_functor& operator=(const non_copy_functor&) = delete;
    non_copy_functor(non_copy_functor&&) = default;
    non_copy_functor& operator=(non_copy_functor&&) = default;
    int operator()() const {
        return 42;
    }
};

struct non_move_functor {
    non_move_functor() = default;
    non_move_functor(const non_move_functor&) = default;
    non_move_functor& operator=(const non_move_functor&) = default;
    non_move_functor(non_move_functor&&) = delete;
    non_move_functor& operator=(non_move_functor&&) = delete;
    int operator()() const {
        return 43;
    }
};

TEST_CASE("make_task_with_non_copy_functor") {
    non_copy_functor functor;
    auto task = make_task(transwarp::root, std::move(functor));
    task->schedule();
    REQUIRE(42 == task->get_future().get());
}

TEST_CASE("make_task_with_non_move_functor") {
    non_move_functor functor;
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    REQUIRE(43 == task->get_future().get());
}

TEST_CASE("make_task_std_function") {
    std::function<int()> functor = [] { return 44; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    REQUIRE(44 == task->get_future().get());
}

int myfunc() {
    return 45;
}

TEST_CASE("make_task_raw_function") {
    auto task = make_task(transwarp::root, myfunc);
    task->schedule();
    REQUIRE(45 == task->get_future().get());
}

struct mock_exec : transwarp::executor {
    bool called = false;
    std::string get_name() const override {
        return "mock_exec";
    }
    void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>&) override {
        called = true;
        functor();
    }
};

TEST_CASE("set_executor_name_and_reset") {
    auto task = make_task(transwarp::root, []{});
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    REQUIRE(exec->get_name() == *task->get_node()->get_executor());
    task->remove_executor();
    REQUIRE_FALSE(task->get_node()->get_executor());
}

TEST_CASE("set_executor_without_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    task->schedule();
    REQUIRE(exec->called);
    REQUIRE(84 == task->get_future().get());
}

TEST_CASE("set_executor_with_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    transwarp::sequential exec_seq;
    task->schedule(exec_seq);
    REQUIRE(exec->called);
    REQUIRE(84 == task->get_future().get());
}

TEST_CASE("remove_executor_with_exec_passed_to_schedule") {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    auto exec = std::make_shared<mock_exec>();
    task->set_executor(exec);
    task->remove_executor();
    mock_exec exec_seq;
    task->schedule(exec_seq);
    REQUIRE_FALSE(exec->called);
    REQUIRE(exec_seq.called);
    REQUIRE(84 == task->get_future().get());
}

TEST_CASE("reset") {
    int value = 42;
    auto functor = [&value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    task->schedule(false);
    REQUIRE(84 == task->get_future().get());
    value = 43;
    task->schedule(false);
    REQUIRE(84 == task->get_future().get());
    task->reset();
    task->schedule(false);
    REQUIRE(86 == task->get_future().get());
}

TEST_CASE("reset_through_schedule") {
    int value = 42;
    auto functor = [&value] { return value*2; };
    auto task = make_task(transwarp::root, functor);
    task->schedule();
    REQUIRE(84 == task->get_future().get());
    value = 43;
    task->schedule(false);
    REQUIRE(84 == task->get_future().get());
    task->schedule();
    REQUIRE(86 == task->get_future().get());
}

TEST_CASE("reset_all") {
    int value = 42;
    auto task = make_task(transwarp::root, [&value] { return value * 2; });
    auto task2 = make_task(transwarp::consume, [](int x) { return x + 3; }, task);
    task2->schedule_all(false);
    REQUIRE(84 == task->get_future().get());
    REQUIRE(87 == task2->get_future().get());
    value = 43;
    task2->schedule_all(false);
    REQUIRE(84 == task->get_future().get());
    REQUIRE(87 == task2->get_future().get());
    task2->reset_all();
    task2->schedule_all(false);
    REQUIRE(86 == task->get_future().get());
    REQUIRE(89 == task2->get_future().get());
}

TEST_CASE("reset_all_through_schedule_all") {
    int value = 42;
    auto task = make_task(transwarp::root, [&value] { return value * 2; });
    auto task2 = make_task(transwarp::consume, [](int x) { return x + 3; }, task);
    task2->schedule_all();
    REQUIRE(84 == task->get_future().get());
    REQUIRE(87 == task2->get_future().get());
    value = 43;
    task2->schedule_all(false);
    REQUIRE(84 == task->get_future().get());
    REQUIRE(87 == task2->get_future().get());
    task2->schedule_all();
    REQUIRE(86 == task->get_future().get());
    REQUIRE(89 == task2->get_future().get());
}

TEST_CASE("consume_any") {
    std::atomic_bool cont(false);
    auto task1 = make_task(transwarp::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto task2 = make_task(transwarp::root, [] {
        return 43;
    });
    auto task3 = make_task(transwarp::consume_any, [](int x) { return x; }, task1, task2);
    transwarp::parallel exec{2};
    task3->schedule_all(exec);
    REQUIRE(43 == task3->get_future().get());
    cont = true;
}

TEST_CASE("wait_any") {
    int result = 0;
    std::atomic_bool cont(false);
    auto task1 = make_task(transwarp::root, [&cont, &result] {
        while (!cont);
        result = 42;
    });
    auto task2 = make_task(transwarp::root, [&result] {
        result = 43;
    });
    auto task3 = make_task(transwarp::wait_any, [] {}, task1, task2);
    transwarp::parallel exec{2};
    task3->schedule_all(exec);
    task3->get_future().wait();
    REQUIRE(43 == result);
    cont = true;
}

TEST_CASE("wait") {
    int result1 = 0;
    int result2 = 0;
    auto task1 = make_task(transwarp::root, [&result1] {
        result1 = 42;
    });
    auto task2 = make_task(transwarp::root, [&result2] {
        result2 = 43;
    });
    auto task3 = make_task(transwarp::wait, [] {}, task1, task2);
    transwarp::parallel exec{2};
    task3->schedule_all(exec);
    task3->get_future().wait();
    REQUIRE(42 == result1);
    REQUIRE(43 == result2);
}

template<typename T>
void make_test_pass_by_reference() {
    using data_t = typename std::decay<T>::type;
    auto data = std::make_shared<data_t>();
    const auto data_ptr = data.get();

    auto t1 = make_task(transwarp::root, [data]() -> T { return *data; });
    auto t2 = make_task(transwarp::consume, [](T d) -> T { return d; }, t1);
    auto t3 = make_task(transwarp::consume_any, [](T d) -> T { return d; }, t2);
    t3->schedule_all();

    auto& result = t3->get_future().get();
    const auto result_ptr = &result;
    REQUIRE(data_ptr == result_ptr);

    auto& result2 = t3->get();
    const auto result2_ptr = &result2;
    REQUIRE(data_ptr == result2_ptr);
}

TEST_CASE("pass_by_reference") {
    using data_t = std::array<double, 10>;
    make_test_pass_by_reference<const data_t&>();
    make_test_pass_by_reference<data_t&>();
}

TEST_CASE("future_throws_task_destroyed") {
    std::shared_future<void> future;
    transwarp::parallel exec{1};
    std::atomic_bool cont{false};
    {
        auto task1 = make_task(transwarp::root, [&cont]{
            while (!cont);
        });
        auto task2 = make_task(transwarp::wait, []{}, task1);
        task2->schedule_all(exec);
        future = task2->get_future();
    }
    cont = true;
    REQUIRE(future.valid());
    REQUIRE_THROWS_AS(future.get(), transwarp::task_destroyed);
}

TEST_CASE("make_task_from_base_task") {
    std::shared_ptr<transwarp::task<int>> t1 = make_task(transwarp::root, []{ return 42; });
    auto t2 = make_task(transwarp::consume, [](int x){ return x; }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->get_future().get());
}

TEST_CASE("schedule_with_two_tasks_wait_with_void_return_method_get") {
    auto f1 = [] {};
    auto task1 = make_task(transwarp::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(transwarp::wait, f2, task1);

    task2->schedule_all();
    task1->get();
    REQUIRE(13 == task2->get());
}

TEST_CASE("task_priority") {
    auto t = make_task(transwarp::root, []{});
    REQUIRE(0 == t->get_node()->get_priority());
    t->set_priority(3);
    REQUIRE(3 == t->get_node()->get_priority());
    t->reset_priority();
    REQUIRE(0 == t->get_node()->get_priority());
}

TEST_CASE("task_custom_data") {
    auto t = make_task(transwarp::root, []{});
    REQUIRE(nullptr == t->get_node()->get_custom_data());
    auto cd = std::make_shared<int>(42);
    t->set_custom_data(cd);
    REQUIRE(cd.get() == t->get_node()->get_custom_data().get());
    t->remove_custom_data();
    REQUIRE(nullptr == t->get_node()->get_custom_data());
}

struct functor : transwarp::functor {

    functor(std::condition_variable& cv, std::mutex& mutex, bool& flag,
            std::atomic_bool& cont, bool& started, bool& ended)
    : cv(cv), mutex(mutex), flag(flag), cont(cont),
      started(started), ended(ended)
    {}

    void operator()() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            flag = true;
        }
        cv.notify_one();
        while (!cont);
        started = true;
        transwarp_cancel_point();
        ended = true;
    }

private:
    std::condition_variable& cv;
    std::mutex& mutex;
    bool& flag;
    std::atomic_bool& cont;
    bool& started;
    bool& ended;
};

TEST_CASE("cancel_task_while_running") {
    transwarp::parallel exec{1};
    std::condition_variable cv;
    std::mutex mutex;
    bool flag = false;
    std::atomic_bool cont{false};
    bool started = false;
    bool ended = false;
    functor f(cv, mutex, flag, cont, started, ended);
    auto task = make_task(transwarp::root, f);
    task->schedule(exec);
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&flag] { return flag; });
    }
    task->cancel(true);
    cont = true;
    task->wait();
    REQUIRE(started);
    REQUIRE_FALSE(ended);
    REQUIRE_THROWS_AS(task->get(), transwarp::task_canceled);
}

TEST_CASE("set_executor_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    auto exec = std::make_shared<transwarp::sequential>();
    t2->set_executor_all(exec);
    REQUIRE(*(t1->get_node()->get_executor()) == exec->get_name());
    REQUIRE(*(t2->get_node()->get_executor()) == exec->get_name());
}

TEST_CASE("remove_executor_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    auto exec = std::make_shared<transwarp::sequential>();
    t2->set_executor_all(exec);
    REQUIRE(*(t1->get_node()->get_executor()) == exec->get_name());
    REQUIRE(*(t2->get_node()->get_executor()) == exec->get_name());
    t2->remove_executor_all();
    REQUIRE_FALSE(t1->get_node()->get_executor());
    REQUIRE_FALSE(t2->get_node()->get_executor());
}

TEST_CASE("set_priority_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    t2->set_priority_all(42);
    REQUIRE(t1->get_node()->get_priority() == 42);
    REQUIRE(t2->get_node()->get_priority() == 42);
}

TEST_CASE("reset_priority_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    t2->set_priority_all(42);
    REQUIRE(t1->get_node()->get_priority() == 42);
    REQUIRE(t2->get_node()->get_priority() == 42);
    t2->reset_priority_all();
    REQUIRE(t1->get_node()->get_priority() == 0);
    REQUIRE(t2->get_node()->get_priority() == 0);
}

TEST_CASE("set_custom_data_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
    REQUIRE(*static_cast<int*>(t1->get_node()->get_custom_data().get()) == *data);
    REQUIRE(*static_cast<int*>(t2->get_node()->get_custom_data().get()) == *data);
}

TEST_CASE("remove_custom_data_all") {
    auto t1 = make_task(transwarp::root, []{});
    auto t2 = make_task(transwarp::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
    REQUIRE(*static_cast<int*>(t1->get_node()->get_custom_data().get()) == *data);
    REQUIRE(*static_cast<int*>(t2->get_node()->get_custom_data().get()) == *data);
    t2->remove_custom_data_all();
    REQUIRE_FALSE(t1->get_node()->get_custom_data());
    REQUIRE_FALSE(t2->get_node()->get_custom_data());
}

TEST_CASE("accept_with_one_parent") {
    auto t1 = make_task(transwarp::root, []{ return 42; });
    auto t2 = make_task(transwarp::accept, [](std::shared_future<int> p1) { return p1.get(); }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->get());
}

TEST_CASE("accept_with_two_parents") {
    auto t1 = make_task(transwarp::root, []{ return 42; });
    auto t2 = make_task(transwarp::root, []{ return 13.3; });
    auto t3 = make_task(transwarp::accept, [](std::shared_future<int> p1, const std::shared_future<double>& p2) { return p1.get() + p2.get(); }, t1, t2);
    t3->schedule_all();
    REQUIRE(55.3 == t3->get());
}

TEST_CASE("accept_any_with_one_parent") {
    auto t1 = make_task(transwarp::root, []{ return 42; });
    auto t2 = make_task(transwarp::accept_any, [](std::shared_future<int> p1) { return p1.get(); }, t1);
    t2->schedule_all();
    REQUIRE(42 == t2->get());
}

TEST_CASE("accept_any_with_two_parents") {
    std::atomic_bool cont(false);
    auto t1 = make_task(transwarp::root, [&cont] {
        while (!cont);
        return 42;
    });
    auto t2 = make_task(transwarp::root, [] {
        return 43;
    });
    auto t3 = make_task(transwarp::accept_any, [](std::shared_future<int> x) { return x.get(); }, t1, t2);
    transwarp::parallel exec{2};
    t3->schedule_all(exec);
    REQUIRE(43 == t3->get_future().get());
    cont = true;
}

TEST_CASE("value_task") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    REQUIRE(42 == t->get_future().get());
    REQUIRE(t->was_scheduled());
    REQUIRE(t->is_ready());
    REQUIRE(t->get_graph().empty());
    auto n = t->get_node();
    REQUIRE(0u == n->get_id());
    REQUIRE(transwarp::task_type::root == n->get_type());
    REQUIRE(!n->get_name());
    REQUIRE(!n->get_executor());
    REQUIRE(n->get_parents().empty());
    REQUIRE(0u == n->get_priority());
    REQUIRE(!n->get_custom_data());
    REQUIRE(!n->is_canceled());
}

TEST_CASE("value_task_with_name") {
    const std::string name = "albert";
    auto t = make_value_task(name, 42);
    REQUIRE(42 == t->get());
    REQUIRE(42 == t->get_future().get());
    REQUIRE(t->was_scheduled());
    REQUIRE(t->is_ready());
    REQUIRE(t->get_graph().empty());
    auto n = t->get_node();
    REQUIRE(0u == n->get_id());
    REQUIRE(transwarp::task_type::root == n->get_type());
    REQUIRE(name == *n->get_name());
    REQUIRE(!n->get_executor());
    REQUIRE(n->get_parents().empty());
    REQUIRE(0u == n->get_priority());
    REQUIRE(!n->get_custom_data());
    REQUIRE(!n->is_canceled());
}

TEST_CASE("value_task_with_priority_and_custom_data") {
    auto t = make_value_task(42);
    t->set_priority(13);
    auto data = std::make_shared<double>(13.5);
    t->set_custom_data(data);
    auto n = t->get_node();
    REQUIRE(13u == n->get_priority());
    REQUIRE(data.get() == n->get_custom_data().get());
    t->remove_custom_data();
    t->reset_priority();
    REQUIRE(0u == n->get_priority());
    REQUIRE(!n->get_custom_data());
}

TEST_CASE("value_task_with_priority_all_and_custom_data_all") {
    auto t = make_value_task(42);
    t->set_priority_all(13);
    auto data = std::make_shared<double>(13.5);
    t->set_custom_data_all(data);
    auto n = t->get_node();
    REQUIRE(13u == n->get_priority());
    REQUIRE(data.get() == n->get_custom_data().get());
    t->remove_custom_data_all();
    t->reset_priority_all();
    REQUIRE(0u == n->get_priority());
    REQUIRE(!n->get_custom_data());
}

TEST_CASE("value_task_in_a_graph") {
    auto t1 = make_value_task(42);
    REQUIRE(42 == t1->get());
    auto t2 = make_value_task(13.3);
    REQUIRE(13.3 == t2->get());
    auto t3 = make_task(transwarp::consume, [](int x, double y) { return x + y; }, t1, t2);
    t3->schedule();
    REQUIRE(55.3 == t3->get());
}

TEST_CASE("value_task_and_executor") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    auto exec = std::make_shared<transwarp::sequential>();
    t->set_executor(exec);
    REQUIRE(!t->get_node()->get_executor());
    t->remove_executor();
    REQUIRE(!t->get_node()->get_executor());
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_executor_all") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    auto exec = std::make_shared<transwarp::sequential>();
    t->set_executor_all(exec);
    REQUIRE(!t->get_node()->get_executor());
    t->remove_executor_all();
    REQUIRE(!t->get_node()->get_executor());
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_schedule") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    transwarp::sequential exec;
    t->schedule(true);
    t->schedule(exec, true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_schedule_all") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    transwarp::sequential exec;
    t->schedule_all(true);
    t->schedule_all(exec, true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_wait") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    t->wait();
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_reset_and_cancel") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    t->reset();
    t->cancel(true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_and_reset_all_and_cancel_all") {
    auto t = make_value_task(42);
    REQUIRE(42 == t->get());
    t->reset_all();
    t->cancel_all(true);
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_with_lvalue_reference") {
    const int x = 42;
    auto t = make_value_task(x);
    REQUIRE(x == t->get());
}

TEST_CASE("value_task_with_rvalue_reference") {
    int x = 42;
    auto t = make_value_task(std::move(x));
    REQUIRE(42 == t->get());
}

TEST_CASE("value_task_with_changing_value") {
    int x = 42;
    auto t = make_value_task(x);
    x = 43;
    REQUIRE(42 == t->get());
}

TEST_CASE("make_ready_future_with_value") {
    const int x = 42;
    auto future = transwarp::detail::make_future_with_value<int>(x);
    REQUIRE(x == future.get());
}

TEST_CASE("make_ready_future") {
    auto future = transwarp::detail::make_ready_future();
    REQUIRE(future.valid());
}

TEST_CASE("make_ready_future_with_exception") {
    std::runtime_error e{"42"};
    auto future = transwarp::detail::make_future_with_exception<int>(std::make_exception_ptr(e));
    try {
        future.get();
    } catch (std::runtime_error& e) {
        REQUIRE(std::string{"42"} == e.what());
        return;
    }
    throw std::runtime_error{"shouldn't get here"};
}

TEST_CASE("make_ready_future_with_invalid_exception") {
    REQUIRE_THROWS_AS(transwarp::detail::make_future_with_exception<int>(std::exception_ptr{}), transwarp::transwarp_error);
}

TEST_CASE("task_set_value_and_remove_value") {
    const int x = 42;
    const int y = 55;
    auto t = make_task(transwarp::root, [x]{ return x; });
    t->schedule();
    REQUIRE(x == t->get());
    t->set_value(y);
    REQUIRE(t->is_ready());
    REQUIRE(y == t->get());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), transwarp::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_set_value_and_remove_value_for_mutable_ref") {
    int x = 42;
    int y = 55;
    auto t = make_task(transwarp::root, [x]{ return x; });
    t->schedule();
    REQUIRE(x == t->get());
    t->set_value(y);
    REQUIRE(t->is_ready());
    REQUIRE(y == t->get());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), transwarp::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_set_value_and_remove_value_for_void") {
    auto t = make_task(transwarp::root, []{});
    t->schedule();
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), transwarp::transwarp_error);
    t->set_value();
    REQUIRE(t->is_ready());
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), transwarp::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
}

TEST_CASE("task_set_exception_and_remove_exception") {
    const int x = 42;
    auto t = make_task(transwarp::root, [x]{ return x; });
    std::runtime_error e{"blah"};
    t->set_exception(std::make_exception_ptr(e));
    REQUIRE(t->is_ready());
    REQUIRE_THROWS_AS(t->get(), std::runtime_error);
    t->reset();
    REQUIRE_THROWS_AS(t->is_ready(), transwarp::transwarp_error);
    t->schedule();
    REQUIRE(t->is_ready());
    REQUIRE(x == t->get());
}

TEST_CASE("task_pass_exception_to_set_value") {
    auto t = make_task(transwarp::root, []{ return std::make_exception_ptr(std::runtime_error{"foo"}); });
    std::runtime_error e{"blah"};
    t->set_value(std::make_exception_ptr(e));
    const auto result = t->get();
    try {
        std::rethrow_exception(result);
    } catch (const std::runtime_error& re) {
        REQUIRE(std::string(e.what()) == std::string(re.what()));
        return;
    }
    throw std::runtime_error{"shouldn't get here"};
}

// Examples

TEST_CASE("example__basic_with_three_tasks") {
    std::ostringstream os;
    examples::basic_with_three_tasks(os);
    const std::string expected = "result = 55.3\nresult = 58.8\n";
    REQUIRE(expected == os.str());
}

void make_test_statistical_keys_facts(bool parallel) {
    std::ostringstream os;
    examples::statistical_key_facts(os, 10000, parallel);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__statistical_key_facts") {
    make_test_statistical_keys_facts(false);
    make_test_statistical_keys_facts(true);
}

TEST_CASE("example__benchmark_simple") {
    std::ostringstream os;
    examples::benchmark_simple(os, 10);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__benchmark_statistical") {
    std::ostringstream os;
    examples::benchmark_statistical(os, 3);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__single_thread_lock_free") {
    std::ostringstream os;
    examples::single_thread_lock_free(os);
    REQUIRE(os.str().size() > 0u);
}
