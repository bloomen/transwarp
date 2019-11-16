#include "single_thread_lock_free.h"
#include "../src/transwarp.h"
#include <array>
#include <iostream>
#include <fstream>
#include <functional>
#include <numeric>
#include <queue>
#include <random>

namespace tw = transwarp;

namespace examples {

namespace {


class executor : public tw::executor {
public:
    executor()
    : done_{false}, thread_{&executor::worker, this}
    {}

    ~executor() {
        done_ = true;
        thread_.join();
    }

    std::string name() const override {
        return "lock_free_executor";
    }

    void execute(const std::function<void()>& functor, tw::itask&) override {
        std::lock_guard<std::mutex> lock{mutex_};
        queue_.push(functor);
    }

private:

    void worker() {
        while (!done_) {
            decltype(queue_) queue;
            {
                std::lock_guard<std::mutex> lock{mutex_};
                queue_.swap(queue);
            }
            while (!queue.empty()) {
                queue.front()();
                queue.pop();
            }
        }
    }

    std::atomic_bool done_;
    std::mutex mutex_; // wouldn't need this if queue_ was lock-free
    std::queue<std::function<void()>> queue_; // this could be a lock-free queue (e.g. boost::spsc_queue)
    std::thread thread_;
};


class data_t {
    using array_t = std::array<double, 1024>;

public:
    data_t() = default;

    data_t(const data_t&) = delete;
    data_t& operator=(const data_t&) = delete;
    data_t(data_t&&) = delete;
    data_t& operator=(data_t&&) = delete;

    array_t::iterator begin() {
        return array_.begin();
    }

    array_t::iterator end() {
        return array_.end();
    }

    array_t::const_iterator begin() const {
        return array_.begin();
    }

    array_t::const_iterator end() const {
        return array_.end();
    }

    std::size_t size() const {
        return array_.size();
    }

private:
    array_t array_;
};


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
// a potentially lock-free, single-thread executor. In addition, it is shown that a stack
// allocated buffer can be used as input into the graph.
void single_thread_lock_free(std::ostream& os, std::size_t sample_size) {
    os.precision(3);

    // Stack allocated buffer
    data_t buffer;

    // Building the graph and retrieving the final task
    auto task = build_graph(buffer);

    // Output the graph for visualization
    std::ofstream("single_thread_lock_free.dot") << tw::to_string(task->edges());

    // The single-thread executor
    executor exec;

    std::size_t count = 0;
    while (count < sample_size) {
        task->schedule_all(exec);
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
