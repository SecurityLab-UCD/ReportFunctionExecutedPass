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

int add(int x, int y) { return x + y; }
int add1(int x) { return add(x, 1); }

int main() {
  int j, n, m, sum;
  n = 5;
  m = add1(add1(n));
  printf("abc\n");
  printf("abc\n");
  for (j = 1; j <= n; j++) {
    m = 3;
    sum = zy(2, m);
    printf("%d\n", sum);
  }
  return 0;
}
