#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <unordered_map>
using namespace std;

static unordered_map<string, int> count_table;

extern "C" int report_count(const char *cstr) {
  string s = (cstr);

  if (count_table.find(s) == count_table.end()) {
    count_table.insert({s, 1});
  } else {
    count_table[s]++;
  }
  return 0;
}

extern "C" int dump_count() {
  cerr << "--- dump record! ---\n";
  for (auto it : count_table) {
    cerr << it.first << ": " << it.second << "\n";
  }
  return 0;
}