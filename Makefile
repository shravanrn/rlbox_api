.PHONY: build

.DEFAULT_GOAL = build

test: test.cpp rlbox.h libtest.cpp libtest.h
	g++ -g -Wall test.cpp libtest.cpp -ldl -o $@

libtest.so: libtest.cpp libtest.h
	g++ -g -shared -fPIC libtest.cpp -o $@

build: test libtest.so