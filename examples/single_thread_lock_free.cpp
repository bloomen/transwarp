#include "single_thread_lock_free.h"
#include "../src/transwarp.h"
#include "readerwriterqueue.h"
#include <fstream>
#include <iostream>

namespace tw = transwarp;

namespace examples {

namespace {

class lock_free_executor : public tw::executor {
public:
    explicit lock_free_executor()
    : thread_(&lock_free_executor::worker, this), queue_(100) {}

    void execute(const std::function<void()>& functor, const std::shared_ptr<transwarp::node>&) {
        queue_.enqueue(functor);
    }

private:

    void worker() {
        for (;;) {
            std::function<void()> functor;
            queue_.wait_dequeue(functor);
            functor();
        }
    }

    std::thread thread_;
    moodycamel::BlockingReaderWriterQueue<std::function<void()>> queue_;
};

}

void single_thread_lock_free(std::ostream& os) {
    (void)os;
}

}

#ifndef USE_LIBUNITTEST
int main() {
    std::cout << "Running example: single_thread_lock_free ..." << std::endl;
    examples::single_thread_lock_free(std::cout);
}
#endif
