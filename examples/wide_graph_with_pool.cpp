#include "wide_graph_with_pool.h"
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


struct graph : tw::graph<double> {
    std::shared_ptr<tw::task<data_t>> input;
    std::shared_ptr<tw::task<double>> final;

    graph(std::shared_ptr<tw::task<data_t>> input,
          std::shared_ptr<tw::task<double>> final)
    : input(std::move(input)),
      final(std::move(final))
    {}

    const std::shared_ptr<transwarp::task<double>>& final_task() const override {
        return final;
    }
};


std::shared_ptr<graph> make_graph() {
    auto input = tw::make_value_task(std::make_shared<std::vector<double>>());
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
    return std::make_shared<graph>(input, final);
}

}


namespace examples {

// This example demonstrates the scheduling of an extra wide graph.
// Increase iterations and size and observe your CPU load.
// New data is scheduled as soon as possible by virtue of a graph pool.
void wide_graph_with_pool(std::ostream& os, std::size_t iterations, std::size_t size) {
    tw::parallel exec{8}; // thread pool with 8 threads

    // Output graph for visualization
    const auto gr = make_graph()->final->get_graph();
    std::ofstream("wide_graph_with_pool.dot") << tw::to_string(gr);

    // This is to generate random data
    std::uniform_int_distribution<std::size_t> dist(size, size * 10);
    std::mt19937 gen{1};

    // Graph pool with 16 initial graphs
    tw::graph_pool<double> pool{make_graph, 16};

    std::vector<std::shared_future<double>> futures;
    for (std::size_t i=0; i<iterations; ++i) {
        auto data = std::make_shared<std::vector<double>>(dist(gen), 1); // New data arrive
        auto g = pool.next_idle_graph<graph>(); // Get the next available graph
        g->input->set_value(data);
        g->final->schedule_all(exec); // Schedule the graph
        futures.push_back(g->final->get_future()); // Collect the future
    }

    // Wait and print results
    for (auto& future : futures) {
        os << future.get() << std::endl;
    }
}

}


#ifndef UNITTEST
int main() {
    std::cout << "Running example: wide_graph_with_pool ..." << std::endl;
    examples::wide_graph_with_pool(std::cout);
}
#endif
