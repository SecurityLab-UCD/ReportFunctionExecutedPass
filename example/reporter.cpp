#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <unordered_map>
using namespace std;

unordered_map<string, int> count_table;

extern "C" int count(const char *cs) {
  // fprintf(stderr, s);
  string s = string(cs);
  if (count_table.find(s) == count_table.end()) {
    count_table.insert({s, 0});
  } else {
    count_table[s]++;
  }
  cerr << s << ": ";
  cerr << count_table[s] << "\n";
  return 0;
}