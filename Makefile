.PHONY: build

.DEFAULT_GOAL = build

test: $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++14 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp $(CURDIR)/RLBox_DynLib.cpp -ldl -o $@

libtest.so: $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++14 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

build: test libtest.so

clean:
	rm -f ./test
	rm -f ./libtest.so