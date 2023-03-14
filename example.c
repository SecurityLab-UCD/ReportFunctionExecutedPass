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

int main() {
  int j, n, m, sum;
  n = 5;
  m = n + 1;
  for (j = 1; j <= n; j++) {
    m = 3;
    sum = zy(2, m);
    printf("%d\n", sum);
  }

  int len = 5;
  int *arr = (int *)malloc(len * sizeof(int));
  for (int i = 0; i < len; i++) {
    arr[i] = i;
  }
  print_arr(NULL, 0);
  print_arr(arr, len);
  free(arr);

  int arr2[5] = {1, 2, 3, 4, 5};
  print_arr(arr2, 5);

  struct Node node;
  node.val1 = 1;
  node.val2 = 2;
  print_node(node);

  test_func_ptr(print_arr);

  int *np = &n;
  test_pointer_pointer(&np);

  return 0;
}
