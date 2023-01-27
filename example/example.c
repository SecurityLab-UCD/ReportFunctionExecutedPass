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

int main() {
  int j, n, m, sum;
  n = 5;
  for (j = 1; j <= n; j++) {
    m = 3;
    sum = zy(2, m);
    printf("%d\n", sum);
  }
}
