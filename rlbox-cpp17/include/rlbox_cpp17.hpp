#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>

#include <boost/pfr/flat.hpp>

namespace rlbox_cpp17 {
namespace detail {
template <typename ToT, typename FromT>
constexpr ToT safe_convert(const FromT &val) {
  using namespace std;
  static_assert(is_fundamental_v<FromT>);
  if constexpr (is_floating_point_v<FromT>) {
    return val;
  } else if constexpr (is_pointer_v<FromT> || is_null_pointer_v<FromT>) {
    return safe_convert<ToT, FromT>(static_cast<uintptr_t>(val));
  } else if constexpr (is_integral_v<FromT>) {
    if constexpr (sizeof(ToT) >= sizeof(FromT)) {
      return val;
    } else {
      assert("Overflow/underflow when converting value to a type with smaller "
             "range" &&
             val <= numeric_limits<ToT>::max() &&
             val >= numeric_limits<ToT>::min());
      return static_cast<ToT>(val);
    }
  } else {
    assert(false && "Unexpected case for safe_convert");
  }
}

template <typename T> constexpr auto extract_struct_members(const T &val) {
  using namespace std;
  static_assert(is_class_v<T>, "val should be class");
  return boost::pfr::flat_structure_to_tuple(val);
}

template <typename T, typename FT>
constexpr auto recursive_apply_members(const T &val, FT f) {
  using namespace std;
  if constexpr (is_fundamental_v<T>) {
    return f(val);
  } else {
    return apply(
        [f](const auto &... args) {
          return make_tuple(recursive_apply_members(args, f)...);
        },
        extract_struct_members(val));
  }
}
} // namespace detail
} // namespace rlbox_cpp17

#include "detail/data_model/lp32.hpp"

// generate to_DATAMODEL(from) -> to function
#define rlbox_cpp17_gen_converter(d_model)                                     \
  template <typename T> constexpr auto to_##d_model(const T &val) {            \
    return detail::recursive_apply_members(val, [](const auto &v) {            \
      return detail::safe_convert<detail::data_model::To##d_model<             \
          std::remove_const_t<std::remove_reference_t<decltype(v)>>>>(v);      \
    });                                                                        \
  }

namespace rlbox_cpp17 {
rlbox_cpp17_gen_converter(LP32);
}
