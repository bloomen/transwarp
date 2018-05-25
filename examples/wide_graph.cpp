#include "wide_graph.h"
#include "../src/transwarp.h"
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
namespace tw = transwarp;

namespace {

std::mt19937 gen{1};

using data_t = std::shared_ptr<std::vector<double>>;

data_t transform(data_t data) {
    std::uniform_real_distribution<double> dist(0.5, 1.5);
    for (auto& v : *data) {
        v *= dist(gen);
    }
    return data;
}

double mean(data_t data) {
    return std::accumulate(data->begin(), data->end(), 0.) / static_cast<double>( data->size() );
}

std::shared_ptr<tw::task<double>> build_graph(std::shared_ptr<tw::task<data_t>> input) {
    auto c0 = tw::make_task(tw::consume, transform, input);
    auto c1 = tw::make_task(tw::consume, transform, input);
    auto c2 = tw::make_task(tw::consume, transform, input);
    auto c3 = tw::make_task(tw::consume, transform, input);
    auto c4 = tw::make_task(tw::consume, transform, input);
    auto c5 = tw::make_task(tw::consume, transform, input);
    auto c6 = tw::make_task(tw::consume, transform, input);
    auto c7 = tw::make_task(tw::consume, transform, input);
    auto d0 = tw::make_task(tw::consume, transform, c0);
    auto d1 = tw::make_task(tw::consume, transform, c1);
    auto d2 = tw::make_task(tw::consume, transform, c2);
    auto d3 = tw::make_task(tw::consume, transform, c3);
    auto d4 = tw::make_task(tw::consume, transform, c4);
    auto d5 = tw::make_task(tw::consume, transform, c5);
    auto d6 = tw::make_task(tw::consume, transform, c6);
    auto d7 = tw::make_task(tw::consume, transform, c7);
    auto final = tw::make_task(tw::consume,
        [](data_t d0, data_t d1, data_t d2, data_t d3, data_t d4, data_t d5, data_t d6, data_t d7) {
            return (mean(d0) + mean(d1) + mean(d2) + mean(d3) + mean(d4) + mean(d5) + mean(d6) + mean(d7)) / 8;
        }, d0, d1, d2, d3, d4, d5, d6, d7);
    return final;
}

}


namespace examples {

// This example demonstrates the scheduling of an extra wide graph.
// Increase iterations and size and observe your CPU load.
// In this example, new data cannot be scheduled until the last result
// was retrieved. However, this can be changed to use a pool of graphs
// to process every data input as soon as possible.
void wide_graph(std::ostream& os, std::size_t iterations, std::size_t size) {
    tw::parallel exec{8}; // thread pool with 8 threads

    // The data input task at the root of the graph
    auto input = tw::make_value_task(std::make_shared<std::vector<double>>());

    // Build graph and return the final task
    auto final = build_graph(input);

    // Output the graph for visualization
    const auto graph = final->get_graph();
    std::ofstream("wide_graph.dot") << tw::to_string(graph);

    // This is to generate random data
    std::uniform_int_distribution<std::size_t> dist(size, size * 10);

    for (std::size_t i=0; i<iterations; ++i) {
        auto data = std::make_shared<std::vector<double>>(dist(gen), 1);
        input->set_value(data); // New data arrive
        final->schedule_all(exec); // Schedule the graph immediately after data arrived
        os << final->get() << std::endl; // Print result
    }
}

}


#ifndef UNITTEST
int main() {
    std::cout << "Running example: wide_graph ..." << std::endl;
    examples::wide_graph(std::cout);
}
#endif
