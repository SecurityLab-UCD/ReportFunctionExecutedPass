CC = clang
CXX = clang++

all: example

reporter.o:
	$(CXX) -g -c reporter.cpp -o reporter.o

lib.o: lib.h
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone lib.c -o lib.ll
	opt -load ../build/report/libReportPass.so -func_report -enable-new-pm=0 -S lib.ll > lib2.ll
	$(CC) -g -c lib2.ll -o lib.o

example: reporter.o lib.o 
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone example.c -o example.ll
	opt -load ../build/report/libReportPass.so -func_report -enable-new-pm=0 -S example.ll > example2.ll
	$(CXX) lib.o reporter.o example2.ll -o example

clean:
	rm *.o example *.ll