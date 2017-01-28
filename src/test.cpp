#include <libunittest/all.hpp>
#include "transwarp.h"
#include <fstream>


COLLECTION(test_transwarp) {

std::string make_dot(const std::vector<transwarp::edge>& graph) {
    auto info = [](transwarp::node n) {
        return '"' + std::to_string(n.id) + "_" + n.name + '"';
    };
    std::ostringstream ofile;
    ofile << "digraph transwarp {" << std::endl;
    for (auto pair : graph) {
        ofile << info(pair.parent) << " -> " << info(pair.child) << std::endl;
    }
    ofile << "}" << std::endl;
    return ofile.str();
}

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

    final->reset();
    final->schedule();
    ASSERT_EQUAL(91, final->get_future().get());

    std::ofstream ofile("basic.dot");
    ofile << make_dot(final->get_graph());
}

TEST(graph) {
    auto f0 = []{ return 0; };
    auto f1 = [](int){ return 0; };
    auto f2 = [](int, int){ return 0; };
    auto f3 = [](int, int, int){ return 0; };
    auto task0 = transwarp::make_task("task0", f0);
    auto task1 = transwarp::make_task("task1", f0);
    auto task2 = transwarp::make_task("task2", f1, task1);
    auto task3 = transwarp::make_task("task3", f2, task2, task0);
    auto task5 = transwarp::make_task("task5", f2, task3, task2);
    auto task6 = transwarp::make_task("task6", f3, task1, task2, task5);
    auto task7 = transwarp::make_task("task7", f2, task5, task6);
    auto task8 = transwarp::make_task("task8", f2, task6, task7);
    auto task9 = transwarp::make_task("task9", f2, task8, task7);
    auto task10 = transwarp::make_task("task10", f2, task9, task8);
    auto task11 = transwarp::make_task("task11", f2, task10, task7);
    auto task12 = transwarp::make_task("task12", f2, task11, task6);
    auto final = transwarp::make_task("final", f3, task10, task11, task12);

    final->finalize();
    std::ofstream ofile("graph.dot");
    ofile << make_dot(final->get_graph());
}

}
