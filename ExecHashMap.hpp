#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

typedef std::vector<std::string> IOVector;
typedef std::pair<IOVector, IOVector> IOPair;

class ExecHashMap {
private:
  struct VectorHasher {
    int operator()(const std::vector<std::string> &V) const {
      std::string all = std::accumulate(
          V.begin(), V.end(), std::string(),
          [&](std::string x, std::string y) { return x + "," + y; });
      return std::hash<std::string>{}(all);
    }
  };

  std::unordered_map<IOVector, std::vector<IOVector>, VectorHasher> map;
  int value_capacity;

public:
  ExecHashMap() : ExecHashMap(0) {}
  ExecHashMap(int cap) : value_capacity(cap) {}

  void insert(const IOPair &io) {
    IOVector inputs = io.first;
    IOVector outputs = io.second;
    if (map[inputs].size() < value_capacity) {
      map[inputs].push_back(outputs);
    }
  }

  std::vector<IOVector> &operator[](const IOVector &xs) { return map[xs]; }

  int size() { return map.size(); }

  nlohmann::json to_json() const {
    nlohmann::json j;
    for (auto &kv : map) {
      j += nlohmann::json{{kv.first, kv.second}};
    }
    return j;
  }
};

class ReportTable {
private:
  std::unordered_map<std::string, ExecHashMap> table;
  int value_capacity;

public:
  ReportTable() {}
  ReportTable(int cap) : value_capacity(cap) {}

  /**
   * @brief Report the input and output of a function to report_table
   * @param func_name: name of the function
   * @param io: input and output of the function
   */
  void report(const std::string &func_name, IOPair &io) {
    if (table.find(func_name) == table.end()) {
      // only report the first 10 executions of the same function
      // ToDo: decide a better upper limit
      auto exec_hash_map = ExecHashMap(value_capacity);
      exec_hash_map.insert(io);
      table.insert({func_name, exec_hash_map});
    } else {
      table[func_name].insert(io);
    }
  }

  nlohmann::json to_json() const {
    nlohmann::json j;
    for (auto &kv : table) {
      j += nlohmann::json{{kv.first, kv.second.to_json()}};
    }
    return j;
  }
};
