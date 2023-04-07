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

struct LinkedNode {
  int value;
  struct LinkedNode *next;
};

struct LinkedNode *create_list(int n) {
  struct LinkedNode *head = NULL;
  for (int i = 0; i < n; i++) {
    struct LinkedNode *node =
        (struct LinkedNode *)malloc(sizeof(struct LinkedNode));
    node->value = i;
    node->next = head;
    head = node;
  }
  return head;
}

void print_list(struct LinkedNode *head) {
  struct LinkedNode *current = head;
  while (current != NULL) {
    printf("%d ", current->value);
    current = current->next;
  }
  putchar('\n');
}

// todo fix this case
static void test_exit(struct LinkedNode *head, int code) { exit(code); }

int main() {
  // struct LargeStruct s = {1, 2, 3.0, 4.0, 5.0, 6.0};
  // double result = foo(s);
  // printf("result = %f\n", result);

  struct LinkedNode *head = create_list(3);
  print_list(head);

  test_exit(head, 0);
  return 0;
}
