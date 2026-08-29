CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

.PHONY: all clean test

all: auctioneer bidder benchmark

auctioneer: src/auctioneer.cpp
	$(CXX) $(CXXFLAGS) src/auctioneer.cpp -o auctioneer

bidder: src/bidder.cpp
	$(CXX) $(CXXFLAGS) src/bidder.cpp -o bidder

benchmark: src/benchmark.cpp
	$(CXX) $(CXXFLAGS) src/benchmark.cpp -o benchmark

test: tests/test_policies.cpp
	$(CXX) $(CXXFLAGS) tests/test_policies.cpp -o test_policies
	./test_policies

clean:
	rm -f auctioneer bidder benchmark test_policies
