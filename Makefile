all: fast

CXXFLAGS = -g -O0 -std=c++14
LINKFLAGS = -lpng -lboost_filesystem -lboost_system

fast: fast.cpp
	$(CXX) -o $@ $(CXXFLAGS) $< $(LINKFLAGS)
