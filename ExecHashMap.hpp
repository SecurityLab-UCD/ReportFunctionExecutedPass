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

/**
 * @brief A hash map that maps inputs to a vector of outputs
 * @details Key is inputs (a vector), value is a vector of outputs
 * (a vector of vectors) of multiple executions
 */
class ExecHashMap {
private:
  struct VectorHasher {
    // https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
    // XOR of each string's hash value
    std::size_t operator()(const std::vector<std::string> &V) const {
      std::hash<std::string> hasher;
      std::size_t seed = 0;
      for (const auto &str : V) {
        seed ^= hasher(str) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
    }
  };

  std::unordered_map<IOVector, std::vector<IOVector>, VectorHasher> map;
  int value_capacity;

public:
  ExecHashMap() : ExecHashMap(0) {}

  /**
   * @brief Construct a new Exec Hash Map object
   * @param cap the capacity of the value vector (maxmium length)
   */
  ExecHashMap(int cap) : value_capacity(cap) {}

  /**
   * @brief Insert a pair of inputs and outputs to the hash map
   * @param io: a pair of inputs (vector) and outputs (vector)
   */
  void insert(const IOPair &io) {
    IOVector inputs = io.first;
    IOVector outputs = io.second;
    if (map[inputs].size() < value_capacity) {
      map[inputs].push_back(outputs);
    }
  }

  /**
   * @brief Get the outputs of a given input
   * @param xs: inputs
   * @return a vector of outputs
   */
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

  /**
   * @brief Construct a new Report Table object
   * @param cap the capacity of the value vector and report table
   */
  ReportTable(int cap) : value_capacity(cap) {}

  /**
   * @brief Report the input and output of a function to report_table
   * @param func_name: name of the function
   * @param io: a pair inputs and outputs of the function
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
