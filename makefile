CXX=g++
CXXFLAGS=-std=c++17 -fsanitize=address,undefined -fno-sanitize-recover=all -O2 -Wall -Werror -Wsign-compare
CXXAVXFLAGS=-march=native

build_main:
	$(CXX) $(CXXFLAGS) -o main main.cpp

main: build_main
	./main
	rm -f main

build_main_avx:
	$(CXX) $(CXXFLAGS) $(CXXAVXFLAGS) -o main_avx main_avx.cpp

main_avx: build_main_avx
	./main_avx
	rm -f main_avx

build_test:
	$(CXX) $(CXXFLAGS) -o test test_hashmap.cpp

test: build_test
	./test
	rm -f test

build_test_avx:
	$(CXX) $(CXXFLAGS) $(CXXAVXFLAGS) -o test_avx test_hashmap_avx.cpp

test_avx: build_test_avx
	./test_avx
	rm -f test_avx

clean:
	rm -f main test
