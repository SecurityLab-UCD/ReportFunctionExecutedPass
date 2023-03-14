# Report Function Execution

## Requirements

* llvm-14 and clang-14
* cmake 3.1

## Build

### Building the Pass

```sh
mkdir build && cd build
cmake ..
make
cd ..
```

### Building the Reporter

```
# get json lib
mkdir -p include
mkdir -p include/nlohmann
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -O include/nlohmann/json.hpp
make
```

## Usage

Pass instrumentation happens at optimazation step.
Compile source code into llvm-IR, instrument IR with the `opt`, then compile instrumented IR into exec.

```sh
cd example
clang -S -emit-llvm -Xclang -disable-O0-optnone example.c -o example.ll
opt -load ../build/report/libReportPass.so -report -enable-new-pm=0 -S example.ll > example2.ll
clang example2.ll
```

This implimentation is largely inspired by
[Runtime Execution Profiling using LLVM](https://www.cs.cornell.edu/courses/cs6120/2019fa/blog/llvm-profiling/).
