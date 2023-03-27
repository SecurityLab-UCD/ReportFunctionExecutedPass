CC = clang
CXX = clang++

REPORTER_INC = -I$(shell pwd)/include
LLVM_INC = -I$(HOME)/clang+llvm/include
LLVM_LIB = -I$(HOME)/clang+llvm/lib

REPORT_FLAGS = -Xclang -load -Xclang ./report/libReportPass.so -flegacy-pass-manager

all: example

reporter:
	$(CXX) -g -shared -fPIC $(REPORTER_INC) reporter.cpp -o reporter.so

pass:
	$(CXX) -g -shared -fPIC $(LLVM_INC) $(LLVM_LIB) -o report/libReportPass.so report/Report.cpp -fno-rtti

lib.o: lib.h pass
	$(CC) $(REPORT_FLAGS) -g -c lib.c

example: reporter lib.o pass
	$(CC) ./reporter.so $(REPORT_FLAGS) lib.o example.c -o example

clean:
	rm *.o example *.ll out.json