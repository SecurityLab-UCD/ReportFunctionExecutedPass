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

static unordered_map<string, vector<IOPair>> report_table;

void report(string func_name, IOPair io) {
  if (report_table.find(func_name) == report_table.end()) {
    report_table.insert({func_name, {io}});
  } else {
    report_table[func_name].push_back(io);
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

const vector<string> FLOAT_TYPES = {"half",  "bfloat",   "float",    "double",
                                    "fp128", "x86_fp80", "ppc_fp128"};

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

    // NOTE: smaller types will be promoted to larger int/float/long/etc.
    bool is_val_ptr = false;

    if (types[i][0] == 'i' && types[i].find("*") == string::npos) {
      // * Integer Type
      int size = atoi(types[i].substr(1).c_str());
      if (size <= 32) {
        param = to_string(va_arg(args, int));
      } else {
        param = to_string(va_arg(args, long));
      }
    } else if (find(FLOAT_TYPES.begin(), FLOAT_TYPES.end(), types[i]) !=
               FLOAT_TYPES.end()) {
      // * Floating-Point Types
      auto it = find(FLOAT_TYPES.begin(), FLOAT_TYPES.end(), types[i]);
      int fp_idx = it - FLOAT_TYPES.begin();
      if (fp_idx <= 4) {
        param = to_string(va_arg(args, double));
      } else {
        param = to_string(va_arg(args, long double));
      }
    } else if (types[i].find('(') != string::npos) {
      // * function type
      param = "func_pointer";
    } else if (is_int_ptr(types[i])) {
      param = to_string_ptr(va_arg(args, int *));
      is_val_ptr = true;
    } else if (types[i].find("i64*") != string::npos) {
      param = to_string_ptr(va_arg(args, long *));
      is_val_ptr = true;
    } else {
      // other types just use type as input encoding
      param = "Unknown Type Value";
    }

    Value v = Value(param, types[i]);
    if (i == len) {
      outputs.push_back(v);
    } else if (is_val_ptr) {
      // if input is a pointer to a value
      // we consider it also as a output since it might be modified
      outputs.push_back(v);
      inputs.push_back(v);
    } else {
      inputs.push_back(v);
    }
  }
  va_end(args);

  report(func_name, {inputs, outputs});
  return 0;
}