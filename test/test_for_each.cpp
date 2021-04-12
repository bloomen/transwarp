#include "test.h"

TEST_CASE("for_each") {
    std::vector<int> vec = {1, 2, 3};
    auto t = tw::for_each(vec.begin(), vec.end(), [](int& x){ x *= 2; });
    tw::sequential exec;
    t->schedule_all(exec);
    REQUIRE(2 == vec[0]);
    REQUIRE(4 == vec[1]);
    REQUIRE(6 == vec[2]);
}

TEST_CASE("for_each_with_executor") {
    std::vector<int> vec = {1, 2, 3};
    tw::parallel exec{4};
    auto t = tw::for_each(exec, vec.begin(), vec.end(), [](int& x){ x *= 2; });
    t->wait();
    REQUIRE(2 == vec[0]);
    REQUIRE(4 == vec[1]);
    REQUIRE(6 == vec[2]);
}

TEST_CASE("for_each_with_invalid_distance") {
    std::vector<int> vec = {1, 2, 3};
    REQUIRE_THROWS_AS(tw::for_each(vec.begin(), vec.begin(), [](int& x){ x *= 2; }), transwarp::invalid_parameter);
}
