#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// for convenience
using json = nlohmann::json;
using namespace std;

typedef struct Value {
  string value;
  string type;

  Value(string val, string ty) : value(val), type(ty){};
} Value;

void to_json(json &j, const Value &v) {
  j = json{{"value", v.value}, {"type", v.type}};
}

typedef pair<vector<Value>, vector<Value>> IOPair;

void to_json(json &j, const IOPair &io) {
  j = json{{"inputs", io.first}, {"outputs", io.second}};
}

typedef struct Report {
  int exec_cnt;
  vector<IOPair> exec_io;

  Report() : exec_cnt(1), exec_io({}){};

  void add(IOPair io_pair) {
    this->exec_cnt++;
    this->exec_io.push_back(io_pair);
  }
} Report;

void to_json(json &j, const Report &r) {
  j = json{{"exec_cnt", r.exec_cnt}, {"exec_io", r.exec_io}};
}

static unordered_map<string, Report> report_table;

extern "C" void report_count(string func_name) {
  if (report_table.find(func_name) == report_table.end()) {
    report_table.insert({func_name, Report()});
  } else {
    report_table[func_name].exec_cnt++;
  }
}

extern "C" void dump_count() {
  json j = report_table;
  cerr << j << "\n";
}

vector<string> parse_meta(string meta) {
  vector<string> xs;
  string delimiter = ">>=";

  size_t pos = 0;
  string token;
  while ((pos = meta.find(delimiter)) != string::npos) {
    token = meta.substr(0, pos);
    xs.push_back(token);
    meta.erase(0, pos + delimiter.length());
  }
  return xs;
}

template <typename T> string to_string_ptr(const T &ptr) {
  if (!ptr) {
    return string("ptr[]");
  }
  // ! recursive for list
  return "ptr[" + to_string(*ptr) + "]";
}

bool is_int_ptr(string type) {
  return type.find("i1*") != string::npos || type.find("i8*") != string::npos ||
         type.find("i32*") != string::npos;
}

extern "C" int report_param(bool has_rnt, const char *param_meta, int len...) {
  va_list args;
  va_start(args, len);
  vector<string> meta_vec = parse_meta(string(param_meta));
  string func_name = meta_vec[0];
  vector<string> types = vector<string>(meta_vec.begin() + 1, meta_vec.end());

  // parse inputs
  string param;
  vector<Value> inputs({});
  vector<Value> outputs({});
  if (!has_rnt) {
    outputs.push_back(Value("void", "void"));
  }

  for (int i = 0; i < len + (int)has_rnt; i++) {
    // using reference
    // https://www.usna.edu/Users/cs/wcbrown/courses/F19SI413/lab/l13/lab.html#top

    // NOTE: 'bool'/'char' are undefined behavior because arguments will be
    // promoted to 'int'
    if (types[i] == "i1" || types[i] == "i8" || types[i] == "i32") {
      param = to_string(va_arg(args, int));
    } else if (types[i] == "i64") {
      param = to_string(va_arg(args, long));
    } else if (is_int_ptr(types[i])) {
      param = to_string_ptr(va_arg(args, int *));
    } else if (types[i].find("i64*") != string::npos) {
      param = to_string_ptr(va_arg(args, long *));
    } else if (types[i].find('(') != string::npos) { // function type
      param = "func_pointer";
    } else {
      // other types just use type as input encoding
      param = "Unknown Type Value";
    }

    Value v = Value(param, types[i]);
    if (i == len) {
      outputs.push_back(v);
    } else {
      inputs.push_back(v);
    }
  }
  va_end(args);

  report_count(func_name);
  report_table[func_name].add({inputs, outputs});

  return 0;
}