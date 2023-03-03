#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

static unordered_map<string, int> count_table;
// io table, key = function name, value = list of (input, output)
static unordered_map<string, vector<pair<string, string>>> io_table;

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
  cerr << "--- dump io! ---\n";
  for (auto it : io_table) {
    cerr << "- " << it.first << ":\n";
    for (auto io_pair : it.second) {
      cerr << "\t" << io_pair.first << ": " << io_pair.second << "\n";
    }
  }
}

vector<string> parse_meta(string meta) {
  vector<string> xs;
  string delimiter = ",";

  size_t pos = 0;
  string token;
  while ((pos = meta.find(delimiter)) != string::npos) {
    token = meta.substr(0, pos);
    xs.push_back(token);
    meta.erase(0, pos + delimiter.length());
  }
  return xs;
}

extern "C" int report_param(const char *meta, int len...) {
  va_list args;
  va_start(args, len);
  string inputs = "";
  string output = "0"; // ToDo: get actual output of function call
  vector<string> meta_vec = parse_meta(string(meta));
  string func_name = meta_vec[0];
  vector<string> types = vector<string>(meta_vec.begin() + 1, meta_vec.end());

  // parse inputs
  string param;
  for (int i = 0; i < len; i++) {
    if (types[i] == "i32") {
      param = to_string(va_arg(args, int));
    }
    // ToDo: to_string for other types
    inputs += param + ",";
  }
  va_end(args);

  pair<string, string> io_pair(inputs, output);
  if (io_table.find(func_name) == io_table.end()) {
    vector<pair<string, string>> new_io_list({io_pair});
    io_table.insert({func_name, new_io_list});
  } else {
    io_table[func_name].push_back(io_pair);
  }

  return 0;
}