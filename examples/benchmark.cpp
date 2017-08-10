#include "benchmark.h"
#include "../src/transwarp.h"
#include <chrono>
#include <fstream>

namespace tw = transwarp;

namespace examples {

const double expected = 4273.5;

int func0() {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return 42;
}

int func1() {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return 13;
}

int func2(int x, int y) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return x + y;
}

double func3() {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return 77.7;
}

double func4(int x, double y) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    return x * y;
}

void calculate_via_functions() {
    const volatile auto val0 = func0();
    const volatile auto val1 = func1();
    const volatile auto val2 = func2(val0, val1);
    const volatile auto val3 = func3();
    const volatile auto val4 = func4(val2, val3);
    if (val4 != expected) {
        throw std::runtime_error("wrong result");
    }
}

std::shared_ptr<tw::itask<double>> build_graph() {
    auto task0 = tw::make_task(tw::root, func0);
    auto task1 = tw::make_task(tw::root, func1);
    auto task2 = tw::make_task(tw::consume, func2, task0, task1);
    auto task3 = tw::make_task(tw::root, func3);
    auto task4 = tw::make_task(tw::consume, func4, task2, task3);
    return task4;
}

void calculate_via_transwarp(tw::itask<double>& task) {
    task.schedule_all();
    if (task.get_future().get() != expected) {
        throw std::runtime_error("wrong result");
    }
    task.reset_all();
}

template<typename Functor>
std::size_t measure(Functor functor, std::size_t sample_size) {
    const auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i=0; i<sample_size; ++i) {
        functor();
    }
    const auto end = std::chrono::high_resolution_clock::now();
    return static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

// This benchmark compares regular function calls with the transwarp graph
void benchmark(std::ostream& os, std::size_t sample_size) {
    auto task = build_graph();
    std::ofstream("benchmark.dot") << tw::make_dot(task->get_graph());

    const auto func_us = measure([] { calculate_via_functions(); }, sample_size);

    const auto tw_us = measure([task] { calculate_via_transwarp(*task); }, sample_size);

    os << "functions: " << func_us << " us" << std::endl;
    os << "transwarp: " << tw_us << " us" << std::endl;
    os << "difference: " << static_cast<double>(tw_us - func_us) /
                            static_cast<double>(func_us) * 100. << " %" << std::endl;
}

}

#ifndef USE_LIBUNITTEST
int main() {
    std::cout << "Running example: benchmark ..." << std::endl;
    examples::benchmark(std::cout);
}
#endif
