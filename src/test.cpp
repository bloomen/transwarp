#include <libunittest/all.hpp>
#include "transwarp.h"
#include "../examples/basic_with_three_tasks.h"
#include "../examples/statistical_key_facts.h"


using transwarp::make_task;
using transwarp::make_task;


COLLECTION(test_transwarp) {

void make_test_one_task(std::size_t threads) {
    const int value = 42;
    auto f1 = [value]{ return value; };
    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task;
    if (threads > 0) {
        task = make_task(f1);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task = make_task(f1);
        executor = std::make_shared<transwarp::sequential>();
    }
    ASSERT_EQUAL(0u, task->get_node().id);
    ASSERT_EQUAL(0u, task->get_node().level);
    ASSERT_EQUAL(0u, task->get_node().parents.size());
    ASSERT_EQUAL("task", task->get_node().name);
    const auto graph = task->get_graph();
    ASSERT_EQUAL(0u, graph.size());
    task->schedule_all(executor.get());
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
    auto task1 = make_task("t1", 13, f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = make_task("\nt2\t", 42, f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task3;
    if (threads > 0) {
        task3 = make_task("t3 ", f3, task1, task2);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task3 = make_task("t3 ", f3, task1, task2);
        executor = std::make_shared<transwarp::sequential>();
    }

    ASSERT_EQUAL(0u, task1->get_node().id);
    ASSERT_EQUAL(0u, task1->get_node().level);
    ASSERT_EQUAL(0u, task1->get_node().parents.size());
    ASSERT_EQUAL("t1", task1->get_node().name);

    ASSERT_EQUAL(1u, task2->get_node().id);
    ASSERT_EQUAL(1u, task2->get_node().level);
    ASSERT_EQUAL(1u, task2->get_node().parents.size());
    ASSERT_EQUAL("\nt2\t", task2->get_node().name);

    ASSERT_EQUAL(2u, task3->get_node().id);
    ASSERT_EQUAL(2u, task3->get_node().level);
    ASSERT_EQUAL(2u, task3->get_node().parents.size());
    ASSERT_EQUAL("t3 ", task3->get_node().name);

    task3->schedule_all(executor.get());
    ASSERT_EQUAL(89, task3->get_future().get());
    ASSERT_EQUAL(42, task1->get_future().get());

    ++value;

    task3->schedule_all(executor.get());
    ASSERT_EQUAL(91, task3->get_future().get());
    ASSERT_EQUAL(43, task1->get_future().get());

    const auto graph = task3->get_graph();
    ASSERT_EQUAL(3u, graph.size());
    const auto dot_graph = transwarp::make_dot(graph);

    const std::string exp_dot_graph = "digraph {\n"
"\"t1\n"
"id 0 pri 13 lev 0 par 0\" -> \"t2\n"
"id 1 pri 42 lev 1 par 1\"\n"
"\"t1\n"
"id 0 pri 13 lev 0 par 0\" -> \"t3\n"
"id 2 pri 0 lev 2 par 2\"\n"
"\"t2\n"
"id 1 pri 42 lev 1 par 1\" -> \"t3\n"
"id 2 pri 0 lev 2 par 2\"\n"
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

    auto task0 = make_task(f0);
    auto task1 = make_task(f0);
    auto task2 = make_task(f1, task1);
    auto task3 = make_task(f2, task2, task0);
    task3->set_executor(seq);
    auto task5 = make_task("task5", f2, task3, task2);
    auto task6 = make_task(f3, task1, task2, task5);
    auto task7 = make_task(3, f2, task5, task6);
    task7->set_executor(seq);
    auto task8 = make_task(2, f2, task6, task7);
    auto task9 = make_task(f1, task7);
    auto task10 = make_task(0, f1, task9);
    task10->set_executor(seq);
    auto task11 = make_task(f3, task10, task7, task8);
    auto task12 = make_task(f2, task11, task6);

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task13;
    if (threads > 0) {
        task13 = make_task(f3, task10, task11, task12);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task13 = make_task(f3, task10, task11, task12);
        executor = std::make_shared<transwarp::sequential>();
    }

    const auto task0_result = 42;
    const auto task3_result = 168;
    const auto task11_result = 11172;
    const auto exp_result = 42042;

    task13->schedule_all(executor.get());
    ASSERT_EQUAL(exp_result, task13->get_future().get());
    ASSERT_EQUAL(task0_result, task0->get_future().get());
    ASSERT_EQUAL(task3_result, task3->get_future().get());
    ASSERT_EQUAL(task11_result, task11->get_future().get());

    for (auto i=0; i<100; ++i) {
        task13->schedule_all(executor.get());
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
    const transwarp::node node{1, 2, 3, "cool", {}, nullptr};
    try {
        throw transwarp::task_canceled(node);
    } catch (const transwarp::transwarp_error& e) {
        ASSERT_EQUAL(msg, e.what());
    }
}

TEST(make_dot_graph_with_empty_graph) {
    const std::vector<transwarp::edge> graph;
    const auto dot_graph = transwarp::make_dot(graph);
    const std::string exp_dot_graph = "digraph {\n}\n";
    ASSERT_EQUAL(exp_dot_graph, dot_graph);
}

TEST(make_dot_graph_with_three_nodes) {
    const transwarp::node node2{1, 10, 1, "node2", {}, nullptr};
    const transwarp::node node3{2, 11, 1, "node3", {}, nullptr};
    const transwarp::node node1{0, 12, 0, "node1", {&node2, &node3}, nullptr};
    std::vector<transwarp::edge> graph;
    graph.push_back({&node1, &node2});
    graph.push_back({&node1, &node3});
    const auto dot_graph = transwarp::make_dot(graph);
    const std::string exp_dot_graph = "digraph {\n"
"\"node2\n"
"id 1 pri 10 lev 1 par 0\" -> \"node1\n"
"id 0 pri 12 lev 0 par 2\"\n"
"\"node3\n"
"id 2 pri 11 lev 1 par 0\" -> \"node1\n"
"id 0 pri 12 lev 0 par 2\"\n"
"}\n";

    ASSERT_EQUAL(exp_dot_graph, dot_graph);
}

template<typename Task>
constexpr std::size_t n_parents() {
    using result_t = decltype(std::declval<Task>().get_parents());
    return std::tuple_size<typename std::decay<result_t>::type>::value;
}

TEST(get_parents) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(f1);
    static_assert(n_parents<decltype(*task1)>() == 0, "");
    auto f2 = [] { return 13; };
    auto task2 = make_task(f2);
    static_assert(n_parents<decltype(*task2)>() == 0, "");
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(f3, task1, task2);
    static_assert(n_parents<decltype(*task3)>() == 2, "");
    auto tasks = task3->get_parents();
    ASSERT_EQUAL(task1.get(), std::get<0>(tasks).get());
    ASSERT_EQUAL(task2.get(), std::get<1>(tasks).get());
}

TEST(get_node) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(f3, task1, task2);

    // task3
    ASSERT_EQUAL(2, task3->get_node().id);
    ASSERT_EQUAL(1, task3->get_node().level);
    ASSERT_EQUAL("task", task3->get_node().name);
    ASSERT_EQUAL(2u, task3->get_node().parents.size());
    ASSERT_EQUAL(&task1->get_node(), task3->get_node().parents[0]);
    ASSERT_EQUAL(&task2->get_node(), task3->get_node().parents[1]);

    // task1
    ASSERT_EQUAL(0, task1->get_node().id);
    ASSERT_EQUAL(0, task1->get_node().level);
    ASSERT_EQUAL("task", task1->get_node().name);
    ASSERT_EQUAL(0u, task1->get_node().parents.size());

    // task2
    ASSERT_EQUAL(1, task2->get_node().id);
    ASSERT_EQUAL(0, task2->get_node().level);
    ASSERT_EQUAL("task", task2->get_node().name);
    ASSERT_EQUAL(0u, task2->get_node().parents.size());
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
    auto task1 = make_task(f1);
    auto task2 = make_task(f2, task1);

    std::shared_ptr<transwarp::executor> executor;
    std::shared_ptr<transwarp::itask<int>> task3;
    if (threads > 0) {
        task3 = make_task(f3, task2);
        executor = std::make_shared<transwarp::parallel>(threads);
    } else {
        task3 = make_task(f3, task2);
        executor = std::make_shared<transwarp::sequential>();
    }
    task3->schedule_all(executor.get());
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

TEST(cancel_with_schedule_all_called_before_in_parallel_and_uncancel) {
    std::atomic_bool cont(false);
    auto f0 = [&cont] {
       while (!cont) {}
       return 42;
    };
    auto f1 = [] (int x) { return x + 13; };
    auto task1 = make_task(f0);
    auto task2 = make_task(f1, task1);
    transwarp::parallel executor(2);
    task2->schedule_all(&executor);
    task2->set_cancel(true);
    cont = true;
    ASSERT_THROW(transwarp::task_canceled, [task2] { task2->get_future().get(); });
    task2->set_cancel(false);
    task2->schedule_all(&executor);
    ASSERT_EQUAL(55, task2->get_future().get());
}

TEST(cancel_with_schedule_all_called_after) {
    auto f0 = [] { return 42; };
    auto f1 = [] (int x) { return x + 13; };
    auto task1 = make_task(f0);
    auto task2 = make_task(f1, task1);
    task2->set_cancel(true);
    transwarp::sequential executor;
    task2->schedule_all(&executor);
    ASSERT_FALSE(task2->get_future().valid());
}

TEST(itask) {
    std::shared_ptr<transwarp::itask<int>> final;
    {
        auto f0 = [] { return 42; };
        auto f1 = [] (int x) { return x + 13; };
        auto task1 = make_task(f0);
        auto task2 = make_task(f1, task1);
        final = task2;
    }
    transwarp::parallel executor(2);
    final->schedule_all(&executor);
    ASSERT_EQUAL(55, final->get_future().get());
}

TEST(sequenced) {
    transwarp::sequential seq;
    ASSERT_EQUAL("transwarp::sequential", seq.get_name());
    int value = 5;
    auto functor = [&value]{ value *= 2; };
    seq.execute(functor, transwarp::node{1, 2, 3, "cool", {}, nullptr});
    ASSERT_EQUAL(10, value);
}

TEST(parallel) {
    transwarp::parallel par(4);
    ASSERT_EQUAL("transwarp::parallel", par.get_name());
    std::atomic_bool done(false);
    int value = 5;
    auto functor = [&value, &done]{ value *= 2; done = true; };
    par.execute(functor, transwarp::node{1, 2, 3, "cool", {}, nullptr});
    while (!done);
    ASSERT_EQUAL(10, value);
}

TEST(wrapped_packager_make_callback) {
    int value = 42;
    auto packager = [&value]{
        return [&value] { value *= 2; };
    };
    const transwarp::node node{1, 2, 3, "cool", {}, nullptr};
    transwarp::detail::wrapped_packager wp(packager, &node);
    auto callback = wp.make_callback();
    ASSERT_EQUAL(&node, callback.second);
    callback.first();
    ASSERT_EQUAL(84, value);
}

std::vector<std::unique_ptr<transwarp::node>> wp_nodes;

transwarp::detail::wrapped_packager
make_wp(std::size_t level, std::size_t priority, std::size_t id) {
    auto packager = []{ return [] {}; };
    wp_nodes.emplace_back(new transwarp::node{id, priority, level, "cool", {}, nullptr});
    return {packager, wp_nodes.back().get()};
}

TEST(wrapped_packager_operator_less) {
    auto wp1 = make_wp(1, 1, 1);
    auto wp2 = make_wp(2, 1, 1);
    ASSERT_TRUE(wp1 < wp2);

    auto wp3 = make_wp(2, 2, 1);
    auto wp4 = make_wp(2, 1, 1);
    ASSERT_TRUE(wp3 < wp4);

    auto wp5 = make_wp(2, 2, 1);
    auto wp6 = make_wp(2, 2, 2);
    ASSERT_TRUE(wp5 < wp6);
}

TEST(schedule_all_without_executor) {
    int x = 13;
    auto task = make_task([&x]{ x *= 2; });
    task->schedule_all();
    task->get_future().wait();
    ASSERT_EQUAL(26, x);
}

TEST(schedule_all_with_task_specific_executor) {
    int value = 42;
    auto functor = [value] { return value*2; };
    auto task = make_task(functor);
    task->set_executor(std::make_shared<transwarp::sequential>());
    task->schedule_all();
    ASSERT_EQUAL(84, task->get_future().get());
}

TEST(invalid_task_specific_executor) {
    auto task = make_task([]{});
    auto functor = [&task] { task->set_executor(nullptr); };
    ASSERT_THROW(transwarp::transwarp_error, functor);
}

TEST(invalid_parent_task) {
    auto parent = make_task([] { return 42; });
    parent.reset();
    auto functor = [&parent] { make_task([](int) {}, parent); };
    ASSERT_THROW(transwarp::transwarp_error, functor);
}

TEST(parallel_with_zero_threads) {
    auto functor = [] { transwarp::parallel{0}; };
    ASSERT_THROW(transwarp::detail::thread_pool_error, functor);
}

TEST(schedule_single_task) {
    int x = 13;
    auto task = make_task([&x]{ x *= 2; });
    task->schedule();
    ASSERT_EQUAL(26, x);
}

TEST(schedule_with_three_tasks_sequential) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(f3, task1, task2);
    task1->schedule();
    task2->schedule();
    task3->schedule();
    ASSERT_EQUAL(55, task3->get_future().get());
    task3->schedule_all();
    ASSERT_EQUAL(55, task3->get_future().get());
}

TEST(schedule_with_three_tasks_parallel) {
    auto f1 = [] { return 42; };
    auto task1 = make_task(f1);
    auto f2 = [] { return 13; };
    auto task2 = make_task(f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = make_task(f3, task1, task2);
    transwarp::parallel executor{4};
    task1->schedule(&executor);
    task2->schedule(&executor);
    task3->schedule(&executor);
    ASSERT_EQUAL(55, task3->get_future().get());
    task3->schedule_all(&executor);
    ASSERT_EQUAL(55, task3->get_future().get());
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

} // test_examples
} // test_transwarp
