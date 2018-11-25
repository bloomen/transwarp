#include "test.h"
#include "../examples/basic_with_three_tasks.h"
#include "../examples/statistical_key_facts.h"
#include "../examples/benchmark_simple.h"
#include "../examples/benchmark_statistical.h"
#include "../examples/single_thread_lock_free.h"
#include "../examples/wide_graph_with_pool.h"

TEST_CASE("example__basic_with_three_tasks") {
    std::ostringstream os;
    examples::basic_with_three_tasks(os);
    const std::string expected = "result = 55.3\nresult = 58.8\n";
    REQUIRE(expected == os.str());
}

void make_test_statistical_keys_facts(bool parallel) {
    std::ostringstream os;
    examples::statistical_key_facts(os, 10000, parallel);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__statistical_key_facts") {
    make_test_statistical_keys_facts(false);
    make_test_statistical_keys_facts(true);
}

TEST_CASE("example__benchmark_simple") {
    std::ostringstream os;
    examples::benchmark_simple(os, 10);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__benchmark_statistical") {
    std::ostringstream os;
    examples::benchmark_statistical(os, 3);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__single_thread_lock_free") {
    std::ostringstream os;
    examples::single_thread_lock_free(os);
    REQUIRE(os.str().size() > 0u);
}

TEST_CASE("example__wide_graph_with_pool") {
    std::ostringstream os;
    examples::wide_graph_with_pool(os, 3, 1000);
    REQUIRE(os.str().size() > 0u);
}
