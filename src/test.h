#include "catch.hpp"
#include "transwarp.h"
#include <array>
#include <sstream>
namespace tw = transwarp;

#ifdef TRANSWARP_CPP11
inline
bool any_data_ok(const transwarp::any_data& d) {
    return d != nullptr;
}

template<typename T>
T get_any_data(const transwarp::any_data& d) {
    return std::static_pointer_cast<typename T::element_type>(d);
}
#else
inline
bool any_data_ok(const transwarp::any_data& d) {
    return d.has_value();
}

template<typename T>
T get_any_data(const transwarp::any_data& d) {
    return std::any_cast<T>(d);
}
#endif

struct no_op_func {
    template<typename... Args>
    void operator()(Args&&...) const {}
};
