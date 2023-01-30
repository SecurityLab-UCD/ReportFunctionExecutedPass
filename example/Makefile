CC = clang

all: example

reporter.o: reporter.h
	$(CC) -g -c reporter.cpp -o reporter.o

lib.o: lib.h
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone lib.c -o lib.ll
	opt -load ../build/report/libReportPass.so -func_report -enable-new-pm=0 -S lib.ll > lib2.ll
	$(CC) -g -c lib2.ll -o lib.o

example: lib.o 
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone example.c -o example.ll
	opt -load ../build/report/libReportPass.so -func_report -enable-new-pm=0 -S example.ll > example2.ll
	$(CC) lib.o example2.ll -o example

clean:
	rm *.o example *.ll