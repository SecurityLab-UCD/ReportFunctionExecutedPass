CC = clang
CXX = clang++

REPORTER_INC = -I$(shell pwd)/include
LLVM_INC = -I$(HOME)/clang+llvm/include
LLVM_LIB = -I$(HOME)/clang+llvm/lib

REPORT_FLAGS = -Xclang -load -Xclang ./libReportPass.so -flegacy-pass-manager

all: clean example

reporter.o:
	$(CXX) -g $(REPORTER_INC) -c reporter.cpp -o reporter.o
libreporter.so:
	$(CXX) -g -shared -fPIC $(REPORTER_INC) reporter.cpp -o libreporter.so

pass:
	$(CXX) -g -shared -fPIC $(LLVM_INC) $(LLVM_LIB) -o libReportPass.so report/Report.cpp -fno-rtti

lib.o: lib.h pass
	$(CC) $(REPORT_FLAGS) -g -c lib.c

example: reporter.o lib.o pass
	$(CC) -Xclang -disable-O0-optnone $(REPORT_FLAGS) example.c lib.o reporter.o -lstdc++ -o example

clean:
	rm -f *.o example *.ll out.json *.a *.so