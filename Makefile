CC = clang
CXX = clang++

REPORTER_INC = -I$(shell pwd)/include
LLVM_INC = -I$(HOME)/clang+llvm/include
LLVM_LIB = -I$(HOME)/clang+llvm/lib

REPORT_FLAGS = -Xclang -load -Xclang ./report/libReportPass.so -flegacy-pass-manager

all: example

reporter.o:
	$(CXX) -g -c $(REPORTER_INC) reporter.cpp -o reporter.o

pass:
	$(CXX) -g -shared -fPIC $(LLVM_INC) $(LLVM_LIB) -o report/libReportPass.so report/Report.cpp -fno-rtti

lib.o: lib.h pass
	$(CC) $(REPORT_FLAGS) -g -c lib.c

example: reporter.o lib.o pass
	$(CXX) reporter.o $(REPORT_FLAGS) lib.o example.c -o example

clean:
	rm *.o example *.ll out.json