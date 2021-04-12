#include "test.h"

TEST_CASE("make_dot_graph_with_empty_graph") {
    const std::vector<tw::edge> graph;
    const auto dot_graph = tw::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n}";
    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("task_type_to_string") {
    REQUIRE("root" == tw::to_string(tw::task_type::root));
    REQUIRE("accept" == tw::to_string(tw::task_type::accept));
    REQUIRE("accept_any" == tw::to_string(tw::task_type::accept_any));
    REQUIRE("consume" == tw::to_string(tw::task_type::consume));
    REQUIRE("consume_any" == tw::to_string(tw::task_type::consume_any));
    REQUIRE("wait" == tw::to_string(tw::task_type::wait));
    REQUIRE("wait_any" == tw::to_string(tw::task_type::wait_any));
}
