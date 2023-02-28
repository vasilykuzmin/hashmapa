CXX=g++
CXXFLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -O2 -Wall -Werror -Wsign-compare


build_main:
	$(CXX) $(CXXFLAGS) -o main main.cpp

main: build_main
	./main
	rm -f main

build_test:
	$(CXX) $(CXXFLAGS) -o test test_hashmap.cpp

test: build_test
	./test
	rm -f test

clean:
	rm -f main test
