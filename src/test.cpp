#include <libunittest/all.hpp>
#include "transwarp.h"
#include <fstream>


COLLECTION(test_transwarp) {

TEST(basic) {
    int value = 42;

    auto f1 = [&value]{ return value; };
    auto task1 = transwarp::make_task("t1", f1);

    auto f2 = [](int v) { return v + 2; };
    auto task2 = transwarp::make_task("t2", f2, task1);

    auto f3 = [](int v, int w) { return v + w + 3; }; 
    auto final = transwarp::make_task("t3", f3, task1, task2);

    final->finalize();
    final->set_parallel(4);

    final->schedule();
    ASSERT_EQUAL(89, final->get_future().get());

    ++value;

    final->schedule();
    ASSERT_EQUAL(91, final->get_future().get());

    std::ofstream ofile("basic.dot");
    ofile << transwarp::make_dot_graph(final->get_graph());
}

TEST(graph) {
    auto f0 = []{ return 0; };
    auto f1 = [](int){ return 0; };
    auto f2 = [](int, int){ return 0; };
    auto f3 = [](int, int, int){ return 0; };
    auto task0 = transwarp::make_task("compute stuff", f0);
    auto task1 = transwarp::make_task("wicked task", f0);
    auto task2 = transwarp::make_task("generating random data", f1, task1);
    auto task3 = transwarp::make_task("task3", f2, task2, task0);
    auto task5 = transwarp::make_task("task5", f2, task3, task2);
    auto task6 = transwarp::make_task("task6", f3, task1, task2, task5);
    auto task7 = transwarp::make_task("task7", f2, task5, task6);
    auto task8 = transwarp::make_task(f2, task6, task7);
    auto task9 = transwarp::make_task("task9", f2, task8, task7);
    auto task10 = transwarp::make_task("task10", f2, task9, task8);
    auto task11 = transwarp::make_task("task11", f2, task10, task7);
    auto task12 = transwarp::make_task("task12", f2, task11, task6);
    auto final = transwarp::make_task("final", f3, task10, task11, task12);

    final->finalize();

    final->set_parallel(2);

    final->schedule();

    std::ofstream ofile("graph.dot");
    ofile << transwarp::make_dot_graph(final->get_graph());
}

TEST(big_graph) {
    auto f0 = []{ return 0; };
    auto f1 = [](int){ return 0; };
    auto f2 = [](int, int){ return 0; };
    auto f3 = [](int, int, int){ return 0; };
    auto master = transwarp::make_task("master", f0);

    auto task0 = transwarp::make_task(f1, master);
    auto task1 = transwarp::make_task(f0);
    auto task2 = transwarp::make_task(f1, task1);
    auto task3 = transwarp::make_task(f2, task2, task0);
    auto task5 = transwarp::make_task(f2, task3, task2);
    auto task6 = transwarp::make_task(f3, task1, task2, task5);
    auto task7 = transwarp::make_task(f2, task5, task6);
    auto task8 = transwarp::make_task(f2, task6, task7);
    auto task9 = transwarp::make_task(f2, task8, task7);
    auto task10 = transwarp::make_task(f2, task9, task8);
    auto task11 = transwarp::make_task(f2, task10, task7);
    auto task12 = transwarp::make_task(f2, task11, task6);

    auto tas0 = transwarp::make_task(f1, master);
    auto tas1 = transwarp::make_task(f0);
    auto tas2 = transwarp::make_task(f1, tas1);
    auto tas3 = transwarp::make_task(f2, tas2, tas0);
    auto tas5 = transwarp::make_task(f2, tas3, tas2);
    auto tas6 = transwarp::make_task(f3, tas1, tas2, tas5);
    auto tas7 = transwarp::make_task(f2, tas5, tas6);
    auto tas8 = transwarp::make_task(f2, tas6, tas7);
    auto tas9 = transwarp::make_task(f2, tas8, tas7);
    auto tas10 = transwarp::make_task(f2, tas9, tas8);
    auto tas11 = transwarp::make_task(f2, tas10, tas7);
    auto tas12 = transwarp::make_task(f2, tas11, tas6);

    auto ta2 = transwarp::make_task(f1, master);
    auto ta3 = transwarp::make_task(f1, ta2);
    auto ta5 = transwarp::make_task(f2, ta3, ta2);
    auto ta6 = transwarp::make_task(f2, ta2, ta5);
    auto ta7 = transwarp::make_task(f2, ta5, ta6);
    auto ta8 = transwarp::make_task(f2, ta6, ta7);

    auto final = transwarp::make_task("final", f3, ta8, tas12, task12);

    final->finalize();

    final->set_parallel(3);

    final->schedule();

    std::ofstream ofile("big_graph.dot");
    ofile << transwarp::make_dot_graph(final->get_graph());
}

}
