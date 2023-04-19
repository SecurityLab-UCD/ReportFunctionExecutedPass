#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <setjmp.h>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <vector>

// for convenience
using json = nlohmann::json;
using namespace std;

static bool SILENT_REPORTER = false;
__attribute__((constructor)) static void check_silence() {
  SILENT_REPORTER = (std::getenv("SILENT_REPORTER") != nullptr);
}

static int MAX_REPORT_SIZE = 10;
__attribute__((constructor)) static void check_max_report() {
  MAX_REPORT_SIZE = (std::getenv("MAX_REPORT_SIZE") != nullptr);
}

typedef struct JSONValue {
  string value;
  string type;

  JSONValue(string val, string ty) : value(val), type(ty){};
} JSONValue;

void to_json(json &j, const JSONValue &v) {
  j = json{{"value", v.value}, {"type", v.type}};
}

typedef pair<vector<JSONValue>, vector<JSONValue>> IOPair;

void to_json(json &j, const IOPair &io) {
  j = json{{"inputs", io.first}, {"outputs", io.second}};
}

thread_local unordered_map<string, vector<IOPair>> report_table;

/**
 * @brief Report the input and output of a function to report_table
 * @param func_name: name of the function
 * @param io: input and output of the function
 */
void report(string func_name, IOPair io) {
  if (report_table.find(func_name) == report_table.end()) {
    report_table.insert({func_name, {io}});
  } else if (report_table[func_name].size() < MAX_REPORT_SIZE) {
    // only report the first 10 executions of the same function
    // ToDo: decide a better upper limit
    report_table[func_name].push_back(io);
  }
}

/**
 * @brief Signal handler non-standard exit
 * https://stackoverflow.com/questions/40311937/terminating-a-program-with-calling-atexit-functions-linux
 */
void signal_handler(__attribute__((unused)) const int signum) {
  exit(EXIT_FAILURE);
}

/**
 * @brief Signal handler for segfault
 */
static sigjmp_buf env;
void segfault_handler(int signal_number) { siglongjmp(env, 1); }

// the fuzzer file will be linked to multiple targets
// for each target, the table should be dumped once
thread_local unsigned int dump_counter = 0;
extern "C" void dump_count() {
  if (SILENT_REPORTER)
    return;
  if (dump_counter > 0) {
    return;
  }
  json j = report_table;
  // libFuzzer prints its msg to stderr
  // so for fuzzers we use cout
  // TODO: print to a file maybe
  cout << j << "\n";
  dump_counter++;
}

vector<string> parse_meta(string meta) {
  vector<string> xs;
  string delimiter = ">>=";

  size_t pos = 0;
  string token;
  // TODO: substr is also expensive.
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
    if (!pp) {
      return nullptr;
    }
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

  if (base_type == "void") {
    return "ptr[]: void";
  } else if (is_int(base_type)) {
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
    if (fp_idx < 3) {
      val = to_string(*(float *)ptr);
    } else if (fp_idx == 3) {
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

// current reporting IOPair
thread_local IOPair current_reporting;
void update_current_reporting(bool is_rnt, const vector<JSONValue> &vs,
                              const string &func_name) {
  if (is_rnt) {
    current_reporting.second = vs;
    report(func_name, current_reporting);
  } else {
    current_reporting.first = vs;
  }
}

extern "C" int report_param(bool is_rnt, const char *param_meta, int len...) {
  if (SILENT_REPORTER)
    return 0;
  va_list args;
  va_start(args, len);
  vector<string> meta_vec = parse_meta(string(param_meta));
  string func_name = meta_vec[0];
  // TODO: Remove this copy constructor and manually calculate offset. Copying
  // can be expensive.
  vector<string> types = vector<string>(meta_vec.begin() + 1, meta_vec.end());

  // parse inputs
  string param = "";
  vector<JSONValue> vs{};

  struct sigaction sa;
  sa.sa_handler = segfault_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGSEGV, &sa, nullptr);

  for (int i = 0; i < len; i++) {

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

      // there are some cases that the reported pointer is invalid,
      // this will prevent the fuzzer from crashing
      // ! still not working in qemu even with sigsetjmp
      if (sigsetjmp(env, 1) == 0) {
        if (ptr_level == 1) {
          void *ptr = va_arg(args, void *);
          param = to_string_ptr(ptr, base_type);
        } else {
          void **ptr = va_arg(args, void **);
          param = to_string_ptr(ptr, base_type, ptr_level);
        }
      } else {
        param = "ptr[]: pointer already freed";
      }
    } else if (is_struct(types[0])) {
      // * Struct Type
      // todo: decode as the type of  first element
      param = "a struct";
    } else {
      // other types just use type as input encoding
      param = "Unknown Type Value";
    }

    JSONValue v = JSONValue(param, types[i]);
    vs.push_back(v);
  }
  va_end(args);

  update_current_reporting(is_rnt, vs, func_name);
  return 0;
}