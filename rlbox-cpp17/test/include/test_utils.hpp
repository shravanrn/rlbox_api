#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>
// read from.[offset, offset + sizeof(ToT)] as ToT
template <typename ToT, typename FromT>
ToT get_obj_partial_representation(const FromT &from, std::size_t offset) {
  const char *src = ((const char *)&from) + offset;
  assert(offset + sizeof(ToT) <= sizeof(FromT));
  ToT target{};
  std::memcpy(&target, src, sizeof(ToT));
  return target;
}
