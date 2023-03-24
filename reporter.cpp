#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
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

/**
 * @brief Report the input and output of a function to report_table
 * @param func_name: name of the function
 * @param io: input and output of the function
 */
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

const vector<string> FLOAT_TYPES = {"half",  "bfloat",   "float",    "double",
                                    "fp128", "x86_fp80", "ppc_fp128"};

bool is_int(string type) {
  return type[0] == 'i' && type.find("*") == string::npos;
}

bool is_float(string type) {
  return find(FLOAT_TYPES.begin(), FLOAT_TYPES.end(), type) !=
         FLOAT_TYPES.end();
}

bool is_pointer_ty(string type) { return type.back() == '*'; }

/**
 * todo: support struct
 * https://llvm.org/docs/LangRef.html#structure-type
 * @brief Check if a type is a struct
 * @param type: type to check
 * @return true if the type is a struct
 */
bool is_struct(string type) { return false; }

void *dereferenceNTimes(void **ptr, int n) {
  void *pp;
  for (int i = 0; i < n; i++) {
    pp = reinterpret_cast<void *>(*ptr);
  }
  return pp;
}

/**
 * @brief Convert a single-level reference pointer to a string of its referent
 * @param ptr: pointer to the referent casted to void*
 * @param base_type: base type of the pointer
 * @return string representation of the referent
 */
string to_string_ptr(void *ptr, string base_type) {
  if (!ptr) {
    return string("ptr[]");
  }
  string val;
  if (is_int(base_type)) {
    // * Integer Type
    int size = atoi(base_type.substr(1).c_str());
    if (size <= 32) {
      val = to_string(*(int *)ptr);
    } else {
      val = to_string(*(long *)ptr);
    }
  } else if (is_float(base_type)) {
    // * Floating-Point Types
    auto it = find(FLOAT_TYPES.begin(), FLOAT_TYPES.end(), base_type);
    int fp_idx = it - FLOAT_TYPES.begin();
    if (fp_idx <= 4) {
      val = to_string(*(double *)ptr);
    } else {
      val = to_string(*(long double *)ptr);
    }
  } else {
    return "ptr[]: base type not supported";
  }
  return val;
}

/**
 * @brief Convert a multi-level reference pointer to a string of its referent
 * @param ptr: pointer to the referent casted to void**
 * @param base_type: base type of the pointer
 * @param ptr_level: number of levels of reference (e.g. int** is ptr_level=2)
 * @return string representation of the referent
 */
string to_string_ptr(void **ptr, string base_type, int ptr_level) {
  if (!ptr) {
    return string("ptr[]");
  }
  string val = "";
  void *deref_ptr = dereferenceNTimes(ptr, ptr_level);
  return "ptr[" + to_string_ptr(deref_ptr, base_type) + "]";
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

    // NOTE: smaller types will be promoted to larger int/float/long/etc.
    bool is_val_ptr = false;

    if (is_int(types[i])) {
      // * Integer Type
      int size = atoi(types[i].substr(1).c_str());
      if (size <= 32) {
        param = to_string(va_arg(args, int));
      } else {
        param = to_string(va_arg(args, long));
      }
    } else if (is_float(types[i])) {
      // * Floating-Point Types
      auto it = find(FLOAT_TYPES.begin(), FLOAT_TYPES.end(), types[i]);
      int fp_idx = it - FLOAT_TYPES.begin();
      if (fp_idx <= 4) {
        param = to_string(va_arg(args, double));
      } else {
        param = to_string(va_arg(args, long double));
      }
    } else if (types[i].find('(') != string::npos) {
      // * Function Type
      param = "func_pointer";
    } else if (is_pointer_ty(types[i])) {
      // * Pointer Type
      // i32**: base_type = i32, ptr_level = 2
      string base_type = "";
      int ptr_level = 0;
      for (char c : types[i]) {
        if (c == '*') {
          ptr_level++;
        } else {
          base_type += c;
        }
      }

      if (ptr_level == 1) {
        void *ptr = va_arg(args, void *);
        param = to_string_ptr(ptr, base_type);
      } else {
        void **ptr = va_arg(args, void **);
        param = to_string_ptr(ptr, base_type, ptr_level);
      }

      is_val_ptr = true;
    } else if (is_struct(types[0])) {
      // * Struct Type
      // todo: decode as the type of  first element
      param = "a struct";
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