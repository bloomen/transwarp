#include "test.h"

TEST_CASE("circular_buffer_buffer_of_capacity_one_no_elements") {
    const std::size_t capacity = 1;
    tw::detail::circular_buffer<double> buffer(capacity);
    REQUIRE(capacity == buffer.capacity());
    REQUIRE(0 == buffer.size());
    REQUIRE(buffer.empty());
}

TEST_CASE("circular_buffer_buffer_of_capacity_one_with_one_added") {
    const std::size_t capacity = 1;
    tw::detail::circular_buffer<double> buffer(capacity);
    REQUIRE(capacity == buffer.capacity());
    const double value = 42;
    buffer.push(value);
    REQUIRE(value == buffer.front());
    REQUIRE(1 == buffer.size());
}

TEST_CASE("circular_buffer_buffer_of_capacity_one_with_two_added") {
    const std::size_t capacity = 1;
    tw::detail::circular_buffer<double> buffer(capacity);
    REQUIRE(capacity == buffer.capacity());
    const double value1 = 42;
    const double value2 = 43.6;
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    REQUIRE(1 == buffer.size());
    buffer.push(value2);
    REQUIRE(value2 == buffer.front());
    REQUIRE(1 == buffer.size());
}

TEST_CASE("circular_buffer_buffer_of_capacity_two_with_two_added") {
    const std::size_t capacity = 2;
    tw::detail::circular_buffer<double> buffer(capacity);
    REQUIRE(capacity == buffer.capacity());
    const double value1 = 42;
    const double value2 = 43.6;
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    REQUIRE(1 == buffer.size());
    buffer.push(value2);
    REQUIRE(value1 == buffer.front());
    REQUIRE(2 == buffer.size());
}

TEST_CASE("circular_buffer_buffer_of_capacity_two_with_three_added") {
    const std::size_t capacity = 2;
    tw::detail::circular_buffer<double> buffer(capacity);
    REQUIRE(capacity == buffer.capacity());
    const double value1 = 42;
    const double value2 = 46;
    const double value3 = 14;
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    REQUIRE(1 == buffer.size());
    buffer.push(value2);
    REQUIRE(value1 == buffer.front());
    REQUIRE(2 == buffer.size());
    buffer.push(value3);
    REQUIRE(value2 == buffer.front());
    REQUIRE(2 == buffer.size());
}

TEST_CASE("circular_buffer_buffer_of_capacity_one_pop") {
    tw::detail::circular_buffer<double> buffer(1);
    const double value1 = 42;
    const double value2 = 46;
    REQUIRE(buffer.empty());
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    buffer.push(value2);
    REQUIRE(value2 == buffer.front());
    buffer.pop();
    REQUIRE(buffer.empty());
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
}

TEST_CASE("circular_buffer_buffer_of_capacity_two_pop") {
    tw::detail::circular_buffer<double> buffer(2);
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
    const double value1 = 42;
    const double value2 = 46;
    buffer.push(value1);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
    buffer.push(value2);
    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.full());
    REQUIRE(value1 == buffer.front());
    buffer.pop();
    REQUIRE(value2 == buffer.front());
    buffer.pop();
    REQUIRE(buffer.empty());
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());
}

TEST_CASE("circular_buffer_buffer_of_capacity_three_with_push_overload") {
    tw::detail::circular_buffer<double> buffer(3);
    const double value1 = 42;
    const double value2 = 46;
    const double value3 = 14;
    const double value4 = 7;
    const double value5 = 8;
    const double value6 = 9;
    const double value7 = 10;
    const double value8 = 11;
    buffer.push(42.);
    REQUIRE(value1 == buffer.front());
    buffer.push(46.);
    REQUIRE(value1 == buffer.front());
    buffer.push(14.);
    REQUIRE(value1 == buffer.front());
    buffer.push(7.);
    REQUIRE(value2 == buffer.front());
    buffer.push(8.);
    REQUIRE(value3 == buffer.front());
    buffer.push(9.);
    REQUIRE(value4 == buffer.front());
    buffer.push(10.);
    REQUIRE(value5 == buffer.front());
    buffer.push(11.);
    REQUIRE(value6 == buffer.front());
    buffer.push(12.);
    REQUIRE(value7 == buffer.front());
    buffer.push(13.);
    REQUIRE(value8 == buffer.front());
}

TEST_CASE("circular_buffer_buffer_of_capacity_three_push_and_pop") {
    tw::detail::circular_buffer<double> buffer(3);
    const double value1 = 42;
    const double value2 = 46;
    const double value3 = 14;
    const double value4 = 7;
    const double value5 = 8;
    const double value6 = 9;
    const double value7 = 10;
    const double value8 = 11;
    REQUIRE(buffer.empty());
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    buffer.pop();
    REQUIRE(buffer.empty());
    buffer.push(value1);
    REQUIRE(value1 == buffer.front());
    buffer.push(value2);
    REQUIRE(value1 == buffer.front());
    buffer.pop();
    REQUIRE(value2 == buffer.front());
    buffer.push(value1);
    buffer.push(value3);
    REQUIRE(value2 == buffer.front());
}
