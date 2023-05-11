#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace FuncReportor {
using namespace std;

typedef struct IOPair {
  vector<string> first;
  vector<string> second;
} IOPair;

typedef struct ExecHashMap {
  struct VectorHasher {
    int operator()(const std::vector<std::string> &V) const {
      std::string all = std::accumulate(
          V.begin(), V.end(), std::string(),
          [&](std::string x, std::string y) { return x + "," + y; });
      return std::hash<std::string>{}(all);
    }
  };

  unordered_map<vector<string>, vector<vector<string>>, VectorHasher> map;
  int value_capacity;

  ExecHashMap() : value_capacity(0) {}
  ExecHashMap(int cap) : value_capacity(cap) {}

  void insert(IOPair p) {
    auto inputs = p.first;
    auto outputs = p.second;

    if (map[inputs].size() < value_capacity) {
      map[inputs].push_back(outputs);
    }
  }

  vector<vector<string>> &operator[](vector<string> xs) { return map[xs]; }

  int size() { return map.size(); }

  string to_json() const {
    nlohmann::json j;
    for (auto &kv : map) {
      j += nlohmann::json{{kv.first, kv.second}};
    }
    return j.dump();
  }
} ExecHashMap;

typedef struct ReportTable {
  unordered_map<string, ExecHashMap> table;

  ReportTable() {}

  /**
   * @brief Report the input and output of a function to report_table
   * @param func_name: name of the function
   * @param io: input and output of the function
   */
  void report(string func_name, IOPair io) {
    if (table.find(func_name) == table.end()) {
      // only report the first 10 executions of the same function
      // ToDo: decide a better upper limit
      auto exec_hash_map = ExecHashMap(10);
      exec_hash_map.insert(io);
      table.insert({func_name, exec_hash_map});
    } else {
      table[func_name].insert(io);
    }
  }

  string to_string() const {
    return "A report table with " + std::to_string(table.size()) + " functions";
  }
} ReportTable;
} // namespace FuncReportor

void to_json(nlohmann::json &j, const FuncReportor::IOPair &io) {
  // (io.first) and (io.first) are vectors of strings
  // use () to avoid dump as {inputs: outputs} if there is only 1 input
  j = nlohmann::json{{(io.first), (io.second)}};
}

void to_json(nlohmann::json &j, const FuncReportor::ReportTable &t) {
  // todo: fix the way to dump the table
  for (auto &kv : t.table) {
    j += nlohmann::json{{kv.first, kv.second.to_json()}};
  }
}