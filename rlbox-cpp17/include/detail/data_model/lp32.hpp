#pragma once
#include "detail/data_model/data_model_base.hpp"

#include <type_traits>

namespace rlbox_cpp17 {
namespace detail {
namespace data_model {

declare_data_model_begin(LP32);
define_conversion_rule_if_t(
    LP32, std::enable_if_t<std::is_pointer_v<T> || std::is_null_pointer_v<T>>,
    uint32_t);
define_conversion_rule(LP32, short, int16_t);
define_conversion_rule(LP32, unsigned short, unsigned int16_t);
define_conversion_rule(LP32, int, int16_t);
define_conversion_rule(LP32, unsigned, uint16_t);
define_conversion_rule(LP32, long, int32_t);
define_conversion_rule(LP32, unsigned long, uint32_t);
define_conversion_rule(LP32, long long, int64_t);
define_conversion_rule(LP32, unsigned long long, uint64_t);
declare_data_model_end(LP32);
// More types can be added here...
} // namespace data_model
} // namespace detail
} // namespace rlbox_cpp17
