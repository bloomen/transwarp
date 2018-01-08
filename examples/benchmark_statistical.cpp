#include "benchmark_statistical.h"
#include "../src/transwarp.h"
#include <chrono>
#include <fstream>
#include <random>
#include <numeric>

namespace tw = transwarp;

namespace {

using data_t = std::shared_ptr<std::vector<double>>;

data_t generate_gamma(std::mt19937& gen) {
    const double alpha = 2;
    const double beta = 2;
    const std::size_t size = 10000;
    auto data = std::make_shared<std::vector<double>>(size);
    std::gamma_distribution<double> dist(alpha, beta);
    for (auto& value : *data) {
        value = dist(gen);
    }
    return data;
}

double average(data_t data) {
    return std::accumulate(data->begin(), data->end(), 0.) / static_cast<double>(data->size());
}

double stddev(data_t data, double average) {
    double sum = 0;
    std::for_each(data->begin(), data->end(),
                  [average,&sum](double x) { sum += std::pow(x - average, 2.); });
    return std::sqrt(sum / static_cast<double>(data->size()));
}

double median(data_t data) {
    auto copy = *data;
    std::sort(copy.begin(), copy.end());
    if(data->size() % 2 == 0)
        return (copy[data->size() / 2 - 1] + copy[data->size() / 2]) / 2;
    else
        return copy[data->size() / 2];
}

int mode(data_t data) {
    auto copy = *data;
    std::sort(copy.begin(), copy.end());
    int number = static_cast<int>(*copy.begin());
    int mode = number;
    int count = 1;
    int count_mode = 1;
    std::for_each(copy.begin() + 1, copy.end(),
                  [&number,&mode,&count,&count_mode](double x) {
                        if (static_cast<int>(x) == number) {
                            ++count;
                        } else {
                            if (count > count_mode) {
                                count_mode = count;
                                mode = number;
                            }
                            count = 1;
                            number = static_cast<int>(x);
                        }
                  });
    return mode;
}

struct result {
    double avg;
    double stddev;
    double median;
    int mode;
};

result aggregate_results(double avg, double stddev, double median, int mode) {
    return {avg, stddev, median, mode};
}

void check_result(const result& res) {
    if (!(res.avg > 0 && res.stddev > 0 && res.median > 0 && res.mode > 0)) {
        throw std::runtime_error("wrong result");
    }
}

void calculate_via_functions(std::mt19937& gen) {
    const auto data = generate_gamma(gen);
    const auto avg = average(data);
    const auto std = stddev(data, avg);
    const auto med = median(data);
    const auto mod = mode(data);
    const auto res = aggregate_results(avg, std, med, mod);
    check_result(res);
}

// cppcheck-suppress passedByValue
std::shared_ptr<tw::task<result>> build_graph(std::shared_ptr<std::mt19937> gen) {
    auto gen_task = tw::make_task(tw::root, [gen]() -> std::mt19937& { return *gen; });
    auto data_task = tw::make_task(tw::consume, generate_gamma, gen_task);

    auto avg_task = tw::make_task(tw::consume, average, data_task);
    auto stddev_task = tw::make_task(tw::consume, stddev, data_task, avg_task);
    auto median_task = tw::make_task(tw::consume, median, data_task);
    auto mode_task = tw::make_task(tw::consume, mode, data_task);

    return tw::make_task(tw::consume, aggregate_results,
                         avg_task, stddev_task, median_task, mode_task);
}

void calculate_via_transwarp(tw::task<result>& task) {
    task.schedule_all();
    const auto& res = task.get();
    check_result(res);
}

template<typename Functor>
long long measure(Functor functor, std::size_t sample_size) {
    const auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i=0; i<sample_size; ++i) {
        functor();
    }
    const auto end = std::chrono::high_resolution_clock::now();
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

}

namespace examples {

// This benchmark compares regular function calls with the transwarp graph
// for a chain of calls to compute statistical measures of a gamma distribution
void benchmark_statistical(std::ostream& os, std::size_t sample_size) {
    auto gen = std::make_shared<std::mt19937>(1);
    auto task = build_graph(gen);
    std::ofstream("benchmark_statistical.dot") << tw::to_string(task->get_graph());

    const auto func_us = measure([gen] { calculate_via_functions(*gen); }, sample_size);

    const auto tw_us = measure([task] { calculate_via_transwarp(*task); }, sample_size);

    os << "functions: " << func_us << " us" << std::endl;
    os << "transwarp: " << tw_us << " us" << std::endl;
    os << "difference: " << static_cast<double>(tw_us - func_us) /
                            static_cast<double>(func_us) * 100. << " %" << std::endl;
}

}

#ifndef UNITTEST
int main() {
    std::cout << "Running example: benchmark_statistical ..." << std::endl;
    examples::benchmark_statistical(std::cout);
}
#endif
