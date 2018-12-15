#include "test.h"

TEST_CASE("make_dot_graph_with_empty_graph") {
    const std::vector<tw::edge> graph;
    const auto dot_graph = tw::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n}";
    REQUIRE(exp_dot_graph == dot_graph);
}

TEST_CASE("make_dot_graph_with_three_nodes") {
    auto node2 = std::make_shared<tw::node>();
    tw::detail::node_manip::set_type(*node2, tw::task_type::consume);
    tw::detail::node_manip::set_name(*node2, std::make_optional(std::string{"node2"}));
    tw::detail::node_manip::set_id(*node2, 1);
    tw::detail::node_manip::set_avg_runtime_us(*node2, 42);
    auto node3 = std::make_shared<tw::node>();
    tw::detail::node_manip::set_type(*node3, tw::task_type::wait);
    tw::detail::node_manip::set_name(*node3, std::make_optional(std::string{"node3"}));
    tw::detail::node_manip::set_id(*node3, 2);
    tw::detail::node_manip::set_executor(*node3, std::make_optional(std::string{"exec"}));
    tw::detail::node_manip::set_avg_waittime_us(*node3, 43);
    auto node1 = std::make_shared<tw::node>();
    tw::detail::node_manip::set_type(*node1, tw::task_type::consume);
    tw::detail::node_manip::set_name(*node1, std::make_optional(std::string{"node1"}));
    tw::detail::node_manip::set_id(*node1, 0);
    tw::detail::node_manip::set_level(*node1, 1);
    tw::detail::node_manip::add_parent(*node1, node2);
    tw::detail::node_manip::add_parent(*node1, node3);
    std::vector<tw::edge> graph;
    graph.emplace_back(node2, node1);
    graph.emplace_back(node3, node1);
    const auto dot_graph = tw::to_string(graph);
    const std::string exp_dot_graph = "digraph {\n"
"\"<node2>\nconsume "
"id=1 lev=0\navg-run-us=42\" -> \"<node1>\nconsume "
"id=0 lev=1\"\n"
"\"<node3>\nwait "
"id=2 lev=0\n<exec>\navg-wait-us=43\" -> \"<node1>\nconsume "
"id=0 lev=1\"\n"
"}";

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
