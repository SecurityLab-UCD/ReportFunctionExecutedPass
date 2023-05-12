#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

typedef struct IOPair {
  std::vector<std::string> first;
  std::vector<std::string> second;
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

  std::unordered_map<std::vector<std::string>,
                     std::vector<std::vector<std::string>>, VectorHasher>
      map;
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

  std::vector<std::vector<std::string>> &
  operator[](std::vector<std::string> xs) {
    return map[xs];
  }

  int size() { return map.size(); }

  nlohmann::json to_json() const {
    nlohmann::json j;
    for (auto &kv : map) {
      j += nlohmann::json{{kv.first, kv.second}};
    }
    return j;
  }
} ExecHashMap;

typedef struct ReportTable {
  std::unordered_map<std::string, ExecHashMap> table;

  ReportTable() {}

  /**
   * @brief Report the input and output of a function to report_table
   * @param func_name: name of the function
   * @param io: input and output of the function
   */
  void report(std::string func_name, IOPair io) {
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

} ReportTable;

void to_json(nlohmann::json &j, const ReportTable &t) {
  for (const auto &kv : t.table) {
    j[kv.first] = kv.second.to_json();
  }
}
