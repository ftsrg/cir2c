// CBMC's goto-cc stops with the invariant "expected string constant" when the
// text arguments of __assert_fail are variables. The arguments of assert() are
// literals, so the output must pass them as literals too.

// cir2c-test-output-lacks: __assert_fail(cast
// cir2c-test-output-contains: __assert_fail("

#include <cassert>

int main(int argc, char **) {
  assert(argc >= 1 && "at least the program name");
  return 0;
}
