#include "rlbox_cpp17.hpp"
#include <vector>

struct TestCase {
  int a = -32765; // 2 in lp32
  // 6B padding
  double b = -2.351; // 8
  char c = '\115';   // 1
  // 3B padding
  long d = 0x35215161;           // 4
  short arr[3] = {-155, 5, 156}; // 2 * 3 = 6
  // 2B padding to align with 8
  std::vector<int> e;
} input;

int main() {
  using namespace std;
  auto lp32_res = rlbox_cpp17::to_LP32(input);
}
