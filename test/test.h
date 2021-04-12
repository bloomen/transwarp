#include "catch.hpp"
#include <transwarp.h>
#include <array>
#include <sstream>
namespace tw = transwarp;

#ifdef TRANSWARP_CPP11
template<typename T>
T get_any_data(const transwarp::any_data& d) {
    return d.get<T>();
}
#else
template<typename T>
T get_any_data(const transwarp::any_data& d) {
    return std::any_cast<T>(d);
}
#endif

inline
bool any_data_ok(const transwarp::any_data& d) {
    return d.has_value();
}

struct no_op_func {
    template<typename... Args>
    void operator()(Args&&...) const {}
};
