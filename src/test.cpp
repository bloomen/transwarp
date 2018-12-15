#define CATCH_CONFIG_MAIN
#include "test.h"

void make_test_one_task(std::size_t threads, tw::schedule_type type) {
    const int value = 42;
    auto f1 = [value]{ return value; };
    std::shared_ptr<tw::executor> executor;
    std::shared_ptr<tw::task<int>> task;
    if (threads > 0) {
        task = tw::make_task(tw::root, f1);
        executor = std::make_shared<tw::parallel>(threads);
    } else {
        task = tw::make_task(tw::root, f1);
        executor = std::make_shared<tw::sequential>();
    }
    REQUIRE(0u == task->get_node()->id());
    REQUIRE(0u == task->get_node()->parents().size());
    REQUIRE_FALSE(task->get_node()->name());
    const auto graph = task->get_graph();
    REQUIRE(0u == graph.size());
    task->schedule_all(*executor, type);
    auto future = task->get_future();
    REQUIRE(42 == future.get());
}

TEST_CASE("one_task_with_schedule_by_depth") {
    auto type = tw::schedule_type::depth;
    make_test_one_task(0, type);
    make_test_one_task(1, type);
    make_test_one_task(2, type);
    make_test_one_task(3, type);
    make_test_one_task(4, type);
}

TEST_CASE("one_task_with_schedule_by_breadth") {
    auto type = tw::schedule_type::breadth;
    make_test_one_task(0, type);
    make_test_one_task(1, type);
    make_test_one_task(2, type);
    make_test_one_task(3, type);
    make_test_one_task(4, type);
}

void make_test_three_tasks(std::size_t threads, tw::schedule_type type) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = tw::make_task(tw::root, "t1", f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = tw::make_task(tw::consume, "t2", f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 

    std::shared_ptr<tw::executor> executor;
    std::shared_ptr<tw::task<int>> task3;
    if (threads > 0) {
        task3 = tw::make_task(tw::consume, "t3", f3, task1, task2);
        executor = std::make_shared<tw::parallel>(threads);
    } else {
        task3 = tw::make_task(tw::consume, "t3", f3, task1, task2);
        executor = std::make_shared<tw::sequential>();
    }

    REQUIRE(0u == task1->get_node()->id());
    REQUIRE(0u == task1->get_node()->parents().size());
    REQUIRE("t1" == *task1->get_node()->name());

    REQUIRE(1u == task2->get_node()->id());
    REQUIRE(1u == task2->get_node()->parents().size());
    REQUIRE("t2" == *task2->get_node()->name());
    task2->set_executor(std::make_shared<tw::sequential>());

    REQUIRE(2u == task3->get_node()->id());
    REQUIRE(2u == task3->get_node()->parents().size());
    REQUIRE("t3" == *task3->get_node()->name());

    REQUIRE_FALSE(task1->was_scheduled());
    REQUIRE_FALSE(task2->was_scheduled());
    REQUIRE_FALSE(task3->was_scheduled());

    REQUIRE_THROWS_AS(task1->is_ready(), tw::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task2->is_ready(), tw::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task3->is_ready(), tw::transwarp_error); // not scheduled yet

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
    const auto dot_graph = tw::to_string(graph);
    std::ofstream{"test.dot"} << dot_graph;

    const std::string exp_dot_graph = "digraph {\n"
"\"<t1>\nroot "
"id=0 lev=0\" -> \"<t2>\nconsume "
"id=1 lev=1\n<transwarp::sequential>\"\n"
"\"<t1>\nroot "
"id=0 lev=0\" -> \"<t3>\nconsume "
"id=2 lev=2\"\n"
"\"<t2>\nconsume "
"id=1 lev=1\n<transwarp::sequential>\" -> \"<t3>\nconsume "
"id=2 lev=2\"\n"
"}";

    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("three_tasks_width_schedule_by_depth") {
    auto type = tw::schedule_type::depth;
    make_test_three_tasks(0, type);
    make_test_three_tasks(1, type);
    make_test_three_tasks(2, type);
    make_test_three_tasks(3, type);
    make_test_three_tasks(4, type);
}

TEST_CASE("three_tasks_width_schedule_by_breadth") {
    auto type = tw::schedule_type::breadth;
    make_test_three_tasks(0, type);
    make_test_three_tasks(1, type);
    make_test_three_tasks(2, type);
    make_test_three_tasks(3, type);
    make_test_three_tasks(4, type);
}

void make_test_bunch_of_tasks(std::size_t threads, tw::schedule_type type) {
    auto f0 = []{ return 42; };
    auto f1 = [](int a){ return 3 * a; };
    auto f2 = [](int a, int b){ return a + b; };
    auto f3 = [](int a, int b, int c){ return a + 2*b + c; };

    auto seq = std::make_shared<tw::sequential>();

    auto task0 = tw::make_task(tw::root, f0);
    auto task1 = tw::make_task(tw::root, f0);
    auto task2 = tw::make_task(tw::consume, f1, task1);
    auto task3 = tw::make_task(tw::consume, f2, task2, task0);
    task3->set_executor(seq);
    auto task5 = tw::make_task(tw::consume, "task5", f2, task3, task2);
    auto task6 = tw::make_task(tw::consume, f3, task1, task2, task5);
    auto task7 = tw::make_task(tw::consume, f2, task5, task6);
    task7->set_executor(seq);
    auto task8 = tw::make_task(tw::consume, f2, task6, task7);
    auto task9 = tw::make_task(tw::consume, f1, task7);
    auto task10 = tw::make_task(tw::consume, f1, task9);
    task10->set_executor(seq);
    auto task11 = tw::make_task(tw::consume, f3, task10, task7, task8);
    auto task12 = tw::make_task(tw::consume, f2, task11, task6);

    std::shared_ptr<tw::executor> executor;
    std::shared_ptr<tw::task<int>> task13;
    if (threads > 0) {
        task13 = tw::make_task(tw::consume, f3, task10, task11, task12);
        executor = std::make_shared<tw::parallel>(threads);
    } else {
        task13 = tw::make_task(tw::consume, f3, task10, task11, task12);
        executor = std::make_shared<tw::sequential>();
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
    auto type = tw::schedule_type::depth;
    make_test_bunch_of_tasks(0, type);
    make_test_bunch_of_tasks(1, type);
    make_test_bunch_of_tasks(2, type);
    make_test_bunch_of_tasks(3, type);
    make_test_bunch_of_tasks(4, type);
}

TEST_CASE("bunch_of_tasks_with_schedule_by_breadth") {
    auto type = tw::schedule_type::breadth;
    make_test_bunch_of_tasks(0, type);
    make_test_bunch_of_tasks(1, type);
    make_test_bunch_of_tasks(2, type);
    make_test_bunch_of_tasks(3, type);
    make_test_bunch_of_tasks(4, type);
}

TEST_CASE("get_node") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = tw::make_task(tw::consume, f3, task1, task2);

    // task3
    REQUIRE(2 == task3->get_node()->id());
    REQUIRE_FALSE(task3->get_node()->name());
    REQUIRE(2u == task3->get_node()->parents().size());
    REQUIRE(task1->get_node().get() == task3->get_node()->parents()[0].get());
    REQUIRE(task2->get_node().get() == task3->get_node()->parents()[1].get());

    // task1
    REQUIRE(0 == task1->get_node()->id());
    REQUIRE_FALSE(task1->get_node()->name());
    REQUIRE(0u == task1->get_node()->parents().size());

    // task2
    REQUIRE(1 == task2->get_node()->id());
    REQUIRE_FALSE(task2->get_node()->name());
    REQUIRE(0u == task2->get_node()->parents().size());
}

TEST_CASE("itask") {
    std::shared_ptr<tw::task<int>> final;
    {
        auto f0 = [] { return 42; };
        auto f1 = [] (int x) { return x + 13; };
        auto task1 = tw::make_task(tw::root, f0);
        auto task2 = tw::make_task(tw::consume, f1, task1);
        final = task2;
    }
    tw::parallel executor(2);
    final->schedule_all(executor);
    REQUIRE(55 == final->get_future().get());
}

TEST_CASE("task_priority") {
    auto t = tw::make_task(tw::root, []{});
    REQUIRE(0 == t->get_node()->priority());
    t->set_priority(3);
    REQUIRE(3 == t->get_node()->priority());
    t->reset_priority();
    REQUIRE(0 == t->get_node()->priority());
}

TEST_CASE("task_custom_data") {
    auto t = tw::make_task(tw::root, []{});
    REQUIRE(nullptr == t->get_node()->custom_data());
    auto cd = std::make_shared<int>(42);
    t->set_custom_data(cd);
    REQUIRE(cd.get() == t->get_node()->custom_data().get());
    t->remove_custom_data();
    REQUIRE(nullptr == t->get_node()->custom_data());
}

TEST_CASE("set_priority_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    t2->set_priority_all(42);
    REQUIRE(t1->get_node()->priority() == 42);
    REQUIRE(t2->get_node()->priority() == 42);
}

TEST_CASE("reset_priority_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    t2->set_priority_all(42);
    REQUIRE(t1->get_node()->priority() == 42);
    REQUIRE(t2->get_node()->priority() == 42);
    t2->reset_priority_all();
    REQUIRE(t1->get_node()->priority() == 0);
    REQUIRE(t2->get_node()->priority() == 0);
}

TEST_CASE("set_custom_data_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
    REQUIRE(*static_cast<int*>(t1->get_node()->custom_data().get()) == *data);
    REQUIRE(*static_cast<int*>(t2->get_node()->custom_data().get()) == *data);
}

TEST_CASE("remove_custom_data_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
    REQUIRE(*static_cast<int*>(t1->get_node()->custom_data().get()) == *data);
    REQUIRE(*static_cast<int*>(t2->get_node()->custom_data().get()) == *data);
    t2->remove_custom_data_all();
    REQUIRE_FALSE(t1->get_node()->custom_data());
    REQUIRE_FALSE(t2->get_node()->custom_data());
}
