CC = clang
CXX = clang++

REPORTER_INC = -I$(shell pwd)/include
LLVM_INC = -I$(HOME)/clang+llvm/include
LLVM_LIB = -I$(HOME)/clang+llvm/lib
all: example

reporter.o:
	$(CXX) -g -c $(REPORTER_INC) reporter.cpp -o reporter.o

pass:
	$(CXX) -g -shared -fPIC $(LLVM_INC) $(LLVM_LIB) -o report/libReportPass.so report/Report.cpp -fno-rtti

lib.o: lib.h pass
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone lib.c -o lib.ll
	opt -load ./report/libReportPass.so -report -enable-new-pm=0 -S lib.ll > lib2.ll
	$(CC) -g -c lib2.ll -o lib.o

example: reporter.o lib.o pass
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone example.c -o example.ll
	opt -load ./report/libReportPass.so -report -enable-new-pm=0 -S example.ll > example2.ll
	$(CXX) lib.o reporter.o example2.ll -o example

clean:
	rm *.o example *.ll out.json