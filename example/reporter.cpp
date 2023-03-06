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

typedef Value Input;
typedef Value Output;
typedef pair<vector<Input>, Output> IOPair;

void to_json(json &j, const IOPair &io) {
  j = json{{"inputs", io.first}, {"output", io.second}};
}

typedef struct Report {
  int exec_cnt;
  vector<IOPair> exec_io;

  Report() : exec_cnt(1), exec_io({}){};

  void add(pair<vector<Input>, Output> io_pair) {
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

extern "C" int report_param(const char *meta, int len...) {
  va_list args;
  va_start(args, len);
  vector<string> meta_vec = parse_meta(string(meta));
  string func_name = meta_vec[0];
  vector<string> types = vector<string>(meta_vec.begin() + 1, meta_vec.end());

  // parse inputs
  string param;
  vector<Value> inputs({});
  Value output = Value("0", "i32"); // ToDo: get actual output of function call
  for (int i = 0; i < len; i++) {
    // using reference
    // https://www.usna.edu/Users/cs/wcbrown/courses/F19SI413/lab/l13/lab.html#top

    // NOTE: 'bool'/'char' are undefined behavior because arguments will be
    // promoted to 'int'
    if (types[i] == "i1" || types[i] == "i8" || types[i] == "i32") {
      param = to_string(va_arg(args, int));
    } else if (types[i] == "i64") {
      param = to_string(va_arg(args, long));
    } else if (types[i] == "i1*" || types[i] == "i8*" || types[i] == "i32*") {
      param = to_string_ptr(va_arg(args, int *));
    } else if (types[i] == "i64*") {
      param = to_string_ptr(va_arg(args, long *));
    } else if (types[i].find('(') != string::npos) { // function type
      param = types[i];
    } else {
      // other types just use type as input encoding
      param = types[i];
    }

    // ToDo: to_string for other types
    inputs.push_back(Value(param, types[i]));
  }
  va_end(args);

  report_count(func_name);
  report_table[func_name].add({inputs, output});

  return 0;
}