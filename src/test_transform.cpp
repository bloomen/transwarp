#include "test.h"

TEST_CASE("transform") {
    const std::vector<int> vec = {1, 2, 3};
    std::vector<int> out(vec.size());
    auto t = tw::transform(vec.begin(), vec.end(), out.begin(), [](int x){ return x * 2; });
    tw::sequential exec;
    t->schedule_all(exec);
    REQUIRE(2 == out[0]);
    REQUIRE(4 == out[1]);
    REQUIRE(6 == out[2]);
}

TEST_CASE("transform_with_executor") {
    const std::vector<int> vec = {1, 2, 3};
    std::vector<int> out(vec.size());
    tw::parallel exec{4};
    auto t = tw::transform(exec, vec.begin(), vec.end(), out.begin(), [](int x){ return x * 2; });
    t->wait();
    REQUIRE(2 == out[0]);
    REQUIRE(4 == out[1]);
    REQUIRE(6 == out[2]);
}

TEST_CASE("transform_with_invalid_distance") {
    const std::vector<int> vec = {1, 2, 3};
    std::vector<int> out(vec.size());
    REQUIRE_THROWS_AS(tw::transform(vec.begin(), vec.begin(), out.begin(), [](int x){ return x * 2; }), transwarp::invalid_parameter);
}
