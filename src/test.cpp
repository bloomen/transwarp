#include <libunittest/all.hpp>
#include "transwarp.h"
#include <fstream>


using transwarp::make_task;


COLLECTION(test_transwarp) {

void make_test_one_task(std::size_t threads) {
    const int value = 42;
    auto f1 = [value]{ return value; };
    auto task = make_task(f1);
    task->finalize();
    task->set_parallel(threads);
    ASSERT_EQUAL(0u, task->get_node().id);
    ASSERT_EQUAL(0u, task->get_node().level);
    ASSERT_EQUAL(0u, task->get_node().parents.size());
    ASSERT_EQUAL("task0", task->get_node().name);
    const auto graph = task->get_graph();
    ASSERT_EQUAL(0u, graph.size());
    task->schedule();
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
    auto task1 = make_task("t1", f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = make_task("t2", f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 
    auto task3 = make_task("t3", f3, task1, task2);

    task3->finalize();
    task3->set_parallel(threads);

    ASSERT_EQUAL(1u, task1->get_node().id);
    ASSERT_EQUAL(2u, task1->get_node().level);
    ASSERT_EQUAL(0u, task1->get_node().parents.size());
    ASSERT_EQUAL("t1", task1->get_node().name);

    ASSERT_EQUAL(2u, task2->get_node().id);
    ASSERT_EQUAL(1u, task2->get_node().level);
    ASSERT_EQUAL(1u, task2->get_node().parents.size());
    ASSERT_EQUAL("t2", task2->get_node().name);

    ASSERT_EQUAL(0u, task3->get_node().id);
    ASSERT_EQUAL(0u, task3->get_node().level);
    ASSERT_EQUAL(2u, task3->get_node().parents.size());
    ASSERT_EQUAL("t3", task3->get_node().name);

    task3->schedule();
    ASSERT_EQUAL(89, task3->get_future().get());
    ASSERT_EQUAL(42, task1->get_future().get());

    ++value;

    task3->schedule();
    ASSERT_EQUAL(91, task3->get_future().get());
    ASSERT_EQUAL(43, task1->get_future().get());

    const auto graph = task3->get_graph();
    ASSERT_EQUAL(3u, graph.size());
    const auto dot_graph = transwarp::make_dot_graph(graph);

    const std::string exp_dot_graph = "digraph transwarp {\n"
"\"t1\n"
"id 1 level 2 parents 0\" -> \"t3\n"
"id 0 level 0 parents 2\"\n"
"\"t2\n"
"id 2 level 1 parents 1\" -> \"t3\n"
"id 0 level 0 parents 2\"\n"
"\"t1\n"
"id 1 level 2 parents 0\" -> \"t2\n"
"id 2 level 1 parents 1\"\n"
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

    auto task0 = make_task(f0);
    auto task1 = make_task(f0);
    auto task2 = make_task(f1, task1);
    auto task3 = make_task(f2, task2, task0);
    auto task5 = make_task(f2, task3, task2);
    auto task6 = make_task(f3, task1, task2, task5);
    auto task7 = make_task(f2, task5, task6);
    auto task8 = make_task(f2, task6, task7);
    auto task9 = make_task(f1, task7);
    auto task10 = make_task(f1, task9);
    auto task11 = make_task(f3, task10, task7, task8);
    auto task12 = make_task(f2, task11, task6);
    auto task13 = make_task(f3, task10, task11, task12);

    task13->finalize();

    const auto exp_result = 42042;

    task13->schedule();
    ASSERT_EQUAL(exp_result, task13->get_future().get());

    task13->set_parallel(threads);

    for (auto i=0; i<100; ++i) {
        task13->schedule();
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


}
