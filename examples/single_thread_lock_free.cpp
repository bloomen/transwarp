#include "single_thread_lock_free.h"
#include "../src/transwarp.h"
#include <fstream>
#include <iostream>
#include <array>
#include <random>
#include <numeric>
#include <boost/lockfree/spsc_queue.hpp>

namespace tw = transwarp;

namespace examples {

namespace {


class lock_free_executor : public tw::executor {
public:
    lock_free_executor()
    : done_(false), queue_(1000), thread_(&lock_free_executor::worker, this) {}

    ~lock_free_executor() {
        done_ = true;
        thread_.join();
    }

    std::string name() const override {
        return "lock_free_executor";
    }

    void execute(const std::function<void()>& functor, const std::shared_ptr<tw::node>&) override {
        queue_.push(functor);
    }

private:

    void worker() {
        for (;;) {
            std::function<void()> functor;
            bool success = false;
            while (!success && !done_) {
                success = queue_.pop(functor);
            }
            if (!success && done_) {
                break;
            }
            functor();
        }
    }

    std::atomic_bool done_;
    boost::lockfree::spsc_queue<std::function<void()>> queue_;
    std::thread thread_;
};


using data_t = std::array<double, 1024>;

const data_t& generate_data(data_t& data, std::shared_ptr<std::mt19937> gen) {
    std::uniform_real_distribution<double> dist;
    for (auto& value : data) {
        value = dist(*gen);
    }
    return data;
}

double average(const data_t& data) {
    return std::accumulate(data.begin(), data.end(), 0.) / static_cast<double>(data.size());
}

double stddev(const data_t& data, double average) {
    double sum = 0;
    std::for_each(data.begin(), data.end(),
                  [average,&sum](double x) { sum += std::pow(x - average, 2.); });
    return std::sqrt(sum / static_cast<double>(data.size()));
}

struct result {
    double avg;
    double stddev;
};

std::ostream& operator<<(std::ostream& os, const result& r) {
    os << "avg=" << r.avg << ", stddev=" << r.stddev;
    return os;
}

std::shared_ptr<tw::task<result>> build_graph(data_t& buffer) {
    auto gen = std::make_shared<std::mt19937>(1);
    auto gen_task = tw::make_value_task(gen)->named("rand gen");
    auto buffer_task = tw::make_task(tw::root, [&buffer]() -> data_t& { return buffer; })->named("buffer");
    auto generator_task = tw::make_task(tw::consume, generate_data, buffer_task, gen_task)->named("generator");
    auto avg_task = tw::make_task(tw::consume, average, generator_task)->named("average");
    auto stddev_task = tw::make_task(tw::consume, stddev, generator_task, avg_task)->named("stddev");
    return tw::make_task(tw::consume, [](double avg, double stddev) { return result{avg, stddev}; },
                         avg_task, stddev_task)->named("aggregation");
}

}

// This example demonstrates how tasks can be scheduled for execution using
// a lock-free, single-thread executor. In addition, it is shown that a stack
// allocated buffer can be used as input into the graph.
void single_thread_lock_free(std::ostream& os, std::size_t sample_size) {
    os.precision(3);

    // Stack allocated buffer
    data_t buffer;

    // Building the graph and retrieving the final task
    auto task = build_graph(buffer);

    // Output the graph for visualization
    std::ofstream("single_thread_lock_free.dot") << tw::to_string(task->edges());

    // The lock-free, single-thread executor
    lock_free_executor executor;

    std::size_t count = 0;
    while (count < sample_size) {
        task->schedule_all(executor);
        os << task->get() << std::endl;
        ++count;
    }
}

}

#ifndef UNITTEST
int main() {
    std::cout << "Running example: single_thread_lock_free ..." << std::endl;
    examples::single_thread_lock_free(std::cout);
}
#endif
