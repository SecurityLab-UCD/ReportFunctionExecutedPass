#include "lib.h"

#include <stdio.h>
#include <stdlib.h>

int id(int x) { return x; }

int zy(int a, int b) {
  int i, c, t = 1;
  c = 0;
  for (i = a; i * i <= b; i++) {
    if (b % i == 0)
      c = zy(i, (b / i)) + c;
  }
  if (i * i > b)
    c = c + 1;
  return (id(c));
}
