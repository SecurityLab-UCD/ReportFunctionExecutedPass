CC = clang

all: example

lib.o: lib.h
	$(CC) -g -O -c lib.c -o lib.o

example: lib.o
	$(CC) -S -emit-llvm -Xclang -disable-O0-optnone example.c -o example.ll
	opt -load ../build/report/libReportPass.so -report -enable-new-pm=0 -S example.ll > example2.ll
	$(CC) lib.o example2.ll -o example

clean:
	rm *.o example *.ll