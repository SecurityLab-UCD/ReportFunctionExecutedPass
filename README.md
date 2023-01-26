# Report Function Execution

## Requirements

* llvm-14 and clang-14
* cmake 3.1

## Build

```sh
mkdir build && cd build
cmake ..
make
cd ..
```

## Run

run with `opt`

```sh
> clang -O0 -emit-llvm example.c -c -o exmaple.bc
> opt -load ./build/report/libReportPass.so -reportpass -enable-new-pm=0 < example.bc > /dev/null
called main with type i32 ()
called zy with type i32 (i32, i32)
```

* *or* [Automatically enable the pass](http://adriansampson.net/blog/clangpass.html)
