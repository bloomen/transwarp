#include "wide_graph_with_pool.h"
#include "../src/transwarp.h"
#include <iostream>
#include <fstream>
#include <random>
#include <numeric>
#include <stack>
#include <list>
namespace tw = transwarp;


namespace transwarp {

template<typename FinalResultType>
class task_bag {
public:
    virtual ~task_bag() = default;

    virtual std::shared_ptr<transwarp::task<FinalResultType>> get_final_task() const = 0;
};

template<typename FinalResultType>
class graph_pool {
public:

    using base_graph = transwarp::task_bag<FinalResultType>;

    graph_pool(std::function<std::shared_ptr<base_graph>()> generator,
               std::size_t minimum_size)
    : generator_(std::move(generator)),
      minimum_(minimum_size)
    {
        init();
    }

    graph_pool(std::function<std::shared_ptr<base_graph>()> generator,
               std::size_t minimum_size,
               std::size_t maximum_size)
    : generator_(std::move(generator)),
      minimum_(minimum_size),
      maximum_(maximum_size)
    {
        init();
    }

    // delete copy/move semantics
    graph_pool(const graph_pool&) = delete;
    graph_pool& operator=(const graph_pool&) = delete;
    graph_pool(graph_pool&&) = delete;
    graph_pool& operator=(graph_pool&&) = delete;

    template<typename Graph>
    std::shared_ptr<Graph> next_idle_graph() {
        static_assert(std::is_base_of<base_graph, Graph>::value, "Graph must be a subclass of transwarp::task_bag");
        if (idle_.empty()) {
            reclaim_finished_graphs();
        }
        if (idle_.empty()) {
            double_pool_size();
        }
        if (idle_.empty()) {
            return nullptr;
        }
        auto g = idle_.top(); idle_.pop();
        busy_.push_back(g);
        return std::dynamic_pointer_cast<Graph>(g);
    }

    std::size_t get_size() const {
        return idle_.size() + busy_.size();
    }

    std::size_t get_idle_count() const {
        return idle_.size();
    }

    std::size_t get_busy_count() const {
        return busy_.size();
    }

    void purge() {
        reclaim_finished_graphs();
        idle_.resize(minimum_);
    }

private:

    void init() {
        if (minimum_ < 1) {
            throw transwarp::invalid_parameter("minimum size must be at least 1");
        }
        if (minimum_ > maximum_) {
            throw transwarp::invalid_parameter("minimum size larger than maximum size");
        }
        for (std::size_t i=0; i<minimum_; ++i) {
            idle_.push(generator_());
        }
    }

    void reclaim_finished_graphs() {
        for (auto it = busy_.begin(); it != busy_.end();) {
            if ((*it)->get_final_task()->has_result()) {
                idle_.push(*it);
                it = busy_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void double_pool_size() {
        const auto pool_size = get_size();
        for (std::size_t i=0; i<pool_size; ++i) {
            if (get_size() == maximum_) {
                break;
            }
            idle_.push(generator_());
        }
    }

    std::function<std::shared_ptr<base_graph>()> generator_;
    std::size_t minimum_;
    std::size_t maximum_ = std::numeric_limits<std::size_t>::max();
    std::stack<std::shared_ptr<base_graph>> idle_;
    std::list<std::shared_ptr<base_graph>> busy_;
};

}


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

struct graph : tw::task_bag<double> {
    std::shared_ptr<tw::task<data_t>> input;
    std::shared_ptr<tw::task<double>> final;

    graph(std::shared_ptr<tw::task<data_t>> input,
          std::shared_ptr<tw::task<double>> final)
    : input(std::move(input)),
      final(std::move(final))
    {}

    std::shared_ptr<transwarp::task<double>> get_final_task() const override {
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

    // Pool of graphs with 16 initial graphs
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
