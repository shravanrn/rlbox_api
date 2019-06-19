#pragma once

// generates an alias template To##DataModel<FromT> which returns TargetT
#define declare_data_model_begin(data_model)                                   \
  namespace detail {                                                           \
  template <typename T, typename = std::void_t<>> struct data_model##Impl {    \
    using type = T;                                                            \
  };                                                                           \
  }

#define define_conversion_rule(data_model, from, to)                           \
  namespace detail {                                                           \
  template <> struct data_model##Impl<from, std::void_t<>> {                   \
    using type = to;                                                           \
  };                                                                           \
  }
#define define_conversion_rule_if_t(data_model, cond, to)                      \
  namespace detail {                                                           \
  template <typename T> struct data_model##Impl<T, std::void_t<cond>> {        \
    using type = to;                                                           \
  };                                                                           \
  }

#define declare_data_model_end(data_model)                                     \
  template <typename T>                                                        \
  using To##data_model = typename detail::data_model##Impl<T>::type;
