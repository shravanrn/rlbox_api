.PHONY: build

.DEFAULT_GOAL = build

test: $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++11 -g -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp -ldl -o $@

libtest.so: $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++11 -g -shared -fPIC $(CURDIR)/libtest.cpp -o $@

build: test libtest.so

clean:
	rm -f ./test
	rm -f ./libtest.so