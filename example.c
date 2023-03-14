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

struct Node {
  int val1;
  int val2;
};

void print_arr(int *arr, int len) {
  for (int i = 0; i < len; i++) {
    printf("%d, ", arr[i]);
  }
  printf("\n");
}

int test_func_ptr(void (*func)(int *, int)) { return 0; }

int test_pointer_pointer(int **pp) { return printf("%d\n", **pp); }

void print_node(struct Node n) { printf("%d %d\n", n.val1, n.val2); }

double add(double a, double b) { return a + b; }

int main() {
  double x = 1.0;
  double y = 2.0;
  double z = add(x, y);
  printf("%f\n", z);
  return 0;
}
