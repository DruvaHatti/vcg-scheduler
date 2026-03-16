CXX= g++
CXXFLAGS= -Wall	-Iinclude

all: auctioneer bidder

auctioneer: src/auctioneer.cpp
	$(CXX) $(CXXFLAGS) src/auctioneer.cpp -o auctioneer

bidder: src/bidder.cpp
	$(CXX) $(CXXFLAGS) src/bidder.cpp -o bidder

clean:
	rm -f auctioneer bidder