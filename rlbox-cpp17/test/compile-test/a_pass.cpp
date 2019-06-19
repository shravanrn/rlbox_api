#include "rlbox_cpp17.hpp"
#include <iostream>
#include <typeinfo>

#include "test_utils.hpp"

struct TestCase {
  int a = -32765; // 2 in lp32
  // 6B padding
  double b = -2.351; // 8
  char c = '\115';   // 1
  // 3B padding
  long d = 0x35215161;           // 4
  short arr[3] = {-155, 5, 156}; // 2 * 3 = 6
                                 // 2B padding to align with 8
} input;

int main() {
  using namespace std;
  auto lp32_res = rlbox_cpp17::to_LP32(input);
  static_assert(sizeof(lp32_res) == 32);

  assert(-32765 == get_obj_partial_representation<int16_t>(lp32_res, 0));
  assert(-2.351 == get_obj_partial_representation<double>(lp32_res, 8));
  assert('\115' == get_obj_partial_representation<char>(lp32_res, 16));
  assert(0x35215161 == get_obj_partial_representation<int32_t>(lp32_res, 20));
  assert(-155 == get_obj_partial_representation<int16_t>(lp32_res, 24));
  assert(5 == get_obj_partial_representation<int16_t>(lp32_res, 26));
  assert(156 == get_obj_partial_representation<int16_t>(lp32_res, 28));
  cout << sizeof(lp32_res) << " " << typeid(lp32_res).name() << endl;
}
