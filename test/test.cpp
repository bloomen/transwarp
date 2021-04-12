#define CATCH_CONFIG_MAIN
#include "test.h"

void make_test_one_task(std::size_t threads) {
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
    REQUIRE(0u == task->id());
    REQUIRE(0u == task->parents().size());
    REQUIRE_FALSE(task->name());
    const auto graph = task->edges();
    REQUIRE(0u == graph.size());
    task->schedule_all(*executor);
    auto future = task->future();
    REQUIRE(42 == future.get());
}

TEST_CASE("one_task_schedule") {
    make_test_one_task(0);
    make_test_one_task(1);
    make_test_one_task(2);
    make_test_one_task(3);
    make_test_one_task(4);
}

void make_test_three_tasks(std::size_t threads) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = tw::make_task(tw::root, f1)->named("t1");

    auto f2 = [](int v) { return v + 2; };
    auto task2 = tw::make_task(tw::consume, f2, task1)->named("t2");

    auto f3 = [](int v, int w) { return v + w + 3; }; 

    std::shared_ptr<tw::executor> executor;
    std::shared_ptr<tw::task<int>> task3;
    if (threads > 0) {
        task3 = tw::make_task(tw::consume, f3, task1, task2)->named("t3");
        executor = std::make_shared<tw::parallel>(threads);
    } else {
        task3 = tw::make_task(tw::consume, f3, task1, task2)->named("t3");
        executor = std::make_shared<tw::sequential>();
    }
    task3->finalize();

    REQUIRE(0u == task1->id());
    REQUIRE(0u == task1->parents().size());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("t1" == *task1->name());
#else
    REQUIRE(!task1->name());
#endif

    REQUIRE(1u == task2->id());
    REQUIRE(1u == task2->parents().size());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("t2" == *task2->name());
#else
    REQUIRE(!task2->name());
#endif
    task2->set_executor(std::make_shared<tw::sequential>());

    REQUIRE(2u == task3->id());
    REQUIRE(2u == task3->parents().size());
#ifndef TRANSWARP_MINIMUM_TASK_SIZE
    REQUIRE("t3" == *task3->name());
#else
    REQUIRE(!task3->name());
#endif

    REQUIRE_FALSE(task1->was_scheduled());
    REQUIRE_FALSE(task2->was_scheduled());
    REQUIRE_FALSE(task3->was_scheduled());

    REQUIRE_THROWS_AS(task1->is_ready(), tw::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task2->is_ready(), tw::transwarp_error); // not scheduled yet
    REQUIRE_THROWS_AS(task3->is_ready(), tw::transwarp_error); // not scheduled yet

    task3->schedule_all(*executor);

    REQUIRE(task1->was_scheduled());
    REQUIRE(task2->was_scheduled());
    REQUIRE(task3->was_scheduled());

    REQUIRE(89 == task3->future().get());
    REQUIRE(42 == task1->future().get());

    REQUIRE(task1->is_ready());
    REQUIRE(task2->is_ready());
    REQUIRE(task3->is_ready());

    ++value;

    task3->schedule_all(*executor);
    REQUIRE(91 == task3->future().get());
    REQUIRE(43 == task1->future().get());

    const auto graph = task3->edges();
    REQUIRE(3u == graph.size());
    const auto dot_graph = tw::to_string(graph);
    std::ofstream{"test.dot"} << dot_graph;

#ifndef TRANSWARP_MINIMUM_TASK_SIZE
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
#else
    const std::string exp_dot_graph = "digraph {\n"
"\"root "
"id=0 lev=0\" -> \"consume "
"id=1 lev=1\n<transwarp::sequential>\"\n"
"\"root "
"id=0 lev=0\" -> \"consume "
"id=2 lev=2\"\n"
"\"consume "
"id=1 lev=1\n<transwarp::sequential>\" -> \"consume "
"id=2 lev=2\"\n"
"}";
#endif
    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("three_tasks_schedule") {
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

    auto seq = std::make_shared<tw::sequential>();

    auto task0 = tw::make_task(tw::root, f0);
    auto task1 = tw::make_task(tw::root, f0);
    auto task2 = tw::make_task(tw::consume, f1, task1);
    auto task3 = tw::make_task(tw::consume, f2, task2, task0);
    task3->set_executor(seq);
    auto task5 = tw::make_task(tw::consume, f2, task3, task2)->named("task5");
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

    task13->schedule_all(*executor);
    REQUIRE(exp_result == task13->future().get());
    REQUIRE(task0_result == task0->future().get());
    REQUIRE(task3_result == task3->future().get());
    REQUIRE(task11_result == task11->future().get());

    for (auto i=0; i<100; ++i) {
        task13->schedule_all(*executor);
        REQUIRE(task0_result == task0->future().get());
        REQUIRE(task3_result == task3->future().get());
        REQUIRE(task11_result == task11->future().get());
        REQUIRE(exp_result == task13->future().get());
    }
}

TEST_CASE("bunch_of_tasks_schedule") {
    make_test_bunch_of_tasks(0);
    make_test_bunch_of_tasks(1);
    make_test_bunch_of_tasks(2);
    make_test_bunch_of_tasks(3);
    make_test_bunch_of_tasks(4);
}

TEST_CASE("task_properties") {
    auto f1 = [] { return 42; };
    auto task1 = tw::make_task(tw::root, f1);
    auto f2 = [] { return 13; };
    auto task2 = tw::make_task(tw::root, f2);
    auto f3 = [](int v, int w) { return v + w; };
    auto task3 = tw::make_task(tw::consume, f3, task1, task2);
    task3->finalize();

    // task3
    REQUIRE(2 == task3->id());
    REQUIRE_FALSE(task3->name());
    REQUIRE(2u == task3->parents().size());
    REQUIRE(task1.get() == task3->parents()[0]);
    REQUIRE(task2.get() == task3->parents()[1]);

    // task1
    REQUIRE(0 == task1->id());
    REQUIRE_FALSE(task1->name());
    REQUIRE(0u == task1->parents().size());

    // task2
    REQUIRE(1 == task2->id());
    REQUIRE_FALSE(task2->name());
    REQUIRE(0u == task2->parents().size());
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
    REQUIRE(55 == final->future().get());
}

TEST_CASE("task_priority") {
    auto t = tw::make_task(tw::root, []{});
    REQUIRE(0 == t->priority());
    t->set_priority(3);
#ifndef TRANSWARP_DISABLE_TASK_PRIORITY
    REQUIRE(3 == t->priority());
#else
    REQUIRE(0 == t->priority());
#endif
    t->reset_priority();
    REQUIRE(0 == t->priority());
}

TEST_CASE("task_custom_data") {
    auto t = tw::make_task(tw::root, []{});
    REQUIRE(!any_data_ok(t->custom_data()));
    auto cd = std::make_shared<int>(42);
    t->set_custom_data(cd);
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(cd.get() == get_any_data<std::shared_ptr<int>>(t->custom_data()).get());
#else
    REQUIRE(!any_data_ok(t->custom_data()));
#endif
    t->remove_custom_data();
    REQUIRE(!any_data_ok(t->custom_data()));
}

TEST_CASE("set_priority_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    t2->set_priority_all(42);
#ifndef TRANSWARP_DISABLE_TASK_PRIORITY
    REQUIRE(t1->priority() == 42);
    REQUIRE(t2->priority() == 42);
#else
    REQUIRE(t1->priority() == 0);
    REQUIRE(t2->priority() == 0);
#endif
}

TEST_CASE("reset_priority_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    t2->set_priority_all(42);
#ifndef TRANSWARP_DISABLE_TASK_PRIORITY
    REQUIRE(t1->priority() == 42);
    REQUIRE(t2->priority() == 42);
#else
    REQUIRE(t1->priority() == 0);
    REQUIRE(t2->priority() == 0);
#endif
    t2->reset_priority_all();
    REQUIRE(t1->priority() == 0);
    REQUIRE(t2->priority() == 0);
}

TEST_CASE("set_custom_data_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(42 == *get_any_data<std::shared_ptr<int>>(t1->custom_data()));
    REQUIRE(42 == *get_any_data<std::shared_ptr<int>>(t2->custom_data()));
#else
    REQUIRE(!any_data_ok(t1->custom_data()));
    REQUIRE(!any_data_ok(t2->custom_data()));
#endif
}

TEST_CASE("remove_custom_data_all") {
    auto t1 = tw::make_task(tw::root, []{});
    auto t2 = tw::make_task(tw::wait, []{}, t1);
    auto data = std::make_shared<int>(42);
    t2->set_custom_data_all(data);
#ifndef TRANSWARP_DISABLE_TASK_CUSTOM_DATA
    REQUIRE(42 == *get_any_data<std::shared_ptr<int>>(t1->custom_data()));
    REQUIRE(42 == *get_any_data<std::shared_ptr<int>>(t2->custom_data()));
#else
    REQUIRE(!any_data_ok(t1->custom_data()));
    REQUIRE(!any_data_ok(t2->custom_data()));
#endif
    t2->remove_custom_data_all();
    REQUIRE(!any_data_ok(t1->custom_data()));
    REQUIRE(!any_data_ok(t2->custom_data()));
}

#ifdef TRANSWARP_CPP11
TEST_CASE("option_str_valid") {
    tw::option_str str{"sweet"};
    REQUIRE(str);
    REQUIRE("sweet" == *str);
}

TEST_CASE("option_str_invalid") {
    tw::option_str str;
    REQUIRE(!str);
}

TEST_CASE("any_data_valid") {
    tw::any_data data{42};
    REQUIRE(data.has_value());
    REQUIRE(42 == data.get<int>());
}

TEST_CASE("any_data_invalid") {
    tw::any_data data;
    REQUIRE(!data.has_value());
}

TEST_CASE("any_data_copy_constructor") {
    const tw::any_data data{42};
    tw::any_data data2(data);
    REQUIRE(data2.has_value());
    REQUIRE(42 == data2.get<int>());
}

TEST_CASE("any_data_copy_constructor_from_invalid") {
    const tw::any_data data;
    tw::any_data data2(data);
    REQUIRE(!data2.has_value());
}

TEST_CASE("any_data_copy_assignment") {
    const tw::any_data data{42};
    tw::any_data data2;
    data2 = data;
    REQUIRE(data2.has_value());
    REQUIRE(42 == data2.get<int>());
}

TEST_CASE("any_data_copy_assignment_from_invalid") {
    const tw::any_data data;
    tw::any_data data2;
    data2 = data;
    REQUIRE(!data2.has_value());
}

TEST_CASE("any_data_move_constructor") {
    tw::any_data data{42};
    tw::any_data data2(std::move(data));
    REQUIRE(data2.has_value());
    REQUIRE(42 == data2.get<int>());
    REQUIRE(!data.has_value());
}

TEST_CASE("any_data_move_constructor_from_invalid") {
    tw::any_data data;
    tw::any_data data2(std::move(data));
    REQUIRE(!data2.has_value());
    REQUIRE(!data.has_value());
}

TEST_CASE("any_data_move_assignment") {
    tw::any_data data{42};
    tw::any_data data2;
    data2 = std::move(data);
    REQUIRE(data2.has_value());
    REQUIRE(42 == data2.get<int>());
    REQUIRE(!data.has_value());
}

TEST_CASE("any_data_move_assignment_from_invalid") {
    tw::any_data data;
    tw::any_data data2;
    data2 = std::move(data);
    REQUIRE(!data2.has_value());
    REQUIRE(!data.has_value());
}
#endif
