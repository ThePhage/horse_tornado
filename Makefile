all: fast

CXXFLAGS = -Wall -g -O0 -std=c++14 -DBOOST_EXCEPTION_DISABLE
LINKFLAGS = -lpng -lboost_filesystem -lboost_system

fast: fast.cpp
	$(CXX) -o $@ $(CXXFLAGS) $< $(LINKFLAGS)

.PHONY: clean
clean:
	rm -rf fast
