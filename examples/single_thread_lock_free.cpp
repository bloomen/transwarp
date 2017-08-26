#include "single_thread_lock_free.h"
#include "../src/transwarp.h"
#include <fstream>
#include <iostream>

namespace tw = transwarp;

namespace examples {

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
