#include "wide_graph_and_cloning.h"
#include "../src/transwarp.h"
#include <iostream>
#include <fstream>
#include <random>
#include <numeric>
namespace tw = transwarp;


namespace {

using data_t = std::shared_ptr<std::vector<double>>;

data_t transform(data_t data) {
    std::uniform_real_distribution<double> dist(0.5, 1.5);
    std::mt19937 gen{static_cast<unsigned int>((*data)[0] * 1000.)};
    for (auto& v : *data) {
        v *= dist(gen);
    }
    return data;
}

data_t copy_transform(data_t data) {
    auto copy = std::make_shared<std::vector<double>>(*data);
    return transform(copy);
}

double mean(data_t data) {
    return std::accumulate(data->begin(), data->end(), 0.) / static_cast<double>(data->size());
}

std::shared_ptr<tw::task<double>> make_graph() {
    auto input = tw::make_value_task(std::make_shared<std::vector<double>>(10));
    std::vector<std::shared_ptr<tw::task<data_t>>> parents;
    for (int i=0; i<8; ++i) {
        auto t = tw::make_task(tw::consume, copy_transform, input)->then(tw::consume, transform);
        parents.emplace_back(t);
    }
    auto final = tw::make_task(tw::consume, [](const std::vector<data_t>& parents) {
                                                double res = 0;
                                                for (const auto p : parents) {
                                                    res += mean(p);
                                                }
                                                return res / static_cast<double>(parents.size());
                                            }, parents);
    return final;
}

}


namespace examples {

// This example demonstrates the scheduling of an extra wide graph including
// task cloning if the current graph is busy.
// Increase iterations and size and observe your CPU load.
void wide_graph_and_cloning(std::ostream& os, std::size_t iterations, std::size_t size) {
    tw::parallel exec{8}; // thread pool with 8 threads

    auto final = make_graph();

    // Output graph for visualization
    const auto gr = final->edges();
    std::ofstream("wide_graph_with_pool.dot") << tw::to_string(gr);

    // To generate random data
    std::uniform_int_distribution<std::size_t> dist(size, size * 10);
    std::mt19937 gen{1};

    std::vector<std::shared_future<double>> futures;
    std::vector<decltype(final)> tasks; // To keep cloned tasks alive
    for (std::size_t i=0; i<iterations; ++i) {
        auto data = std::make_shared<std::vector<double>>(dist(gen), 1); // New data arrive

        if (!final->has_result()) { // Clone if the graph is busy
            final = final->clone();
            tasks.push_back(final);
        }

        auto input = final->tasks()[0];
        static_cast<tw::task<std::shared_ptr<std::vector<double>>>*>(input)->set_value(data);

        final->schedule_all(exec); // Schedule the graph

        futures.push_back(final->future()); // Collect the future
    }

    // Wait and print results
    for (auto& future : futures) {
        os << future.get() << std::endl;
    }
}

}


#ifndef UNITTEST
int main() {
    std::cout << "Running example: wide_graph_and_cloning ..." << std::endl;
    examples::wide_graph_and_cloning(std::cout);
}
#endif
