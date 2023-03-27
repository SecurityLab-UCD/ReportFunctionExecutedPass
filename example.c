#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib.h"

struct LargeStruct {
  int a;
  int b;
  float c;
  float d;
  double e;
  double f;
};

double foo(struct LargeStruct s) {
  double e = s.e;
  double f = s.f;
  return e + f;
}

int main() {
  struct LargeStruct s = {1, 2, 3.0, 4.0, 5.0, 6.0};
  double result = foo(s);
  printf("result = %f\n", result);
  return 0;
}
