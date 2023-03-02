#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <unordered_map>
using namespace std;

static unordered_map<string, int> count_table;

extern "C" int report_count(const char *cstr) {
  string s = string(cstr);

  if (count_table.find(s) == count_table.end()) {
    count_table.insert({s, 1});
  } else {
    count_table[s]++;
  }
  return 0;
}

extern "C" void dump_count() {
  cerr << "--- dump record! ---\n";
  for (auto it : count_table) {
    cerr << it.first << ": " << it.second << "\n";
  }
}

extern "C" int report_param(int len...) {
  va_list args;
  va_start(args, len);
  string s = "";

  for (int i = 0; i < len; i++) {
    int param = va_arg(args, int);
    s += to_string(param);
  }

  va_end(args);
  cout << "=========> " << s << " <=========\n";
  return 0;
}