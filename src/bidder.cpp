#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include "../include/protocol.h"

namespace {
constexpr int PORT = 8080;
constexpr int REBID_INTERVAL_MS = 5000; // should match the auctioneer's bidding window
constexpr int TICK_MS = 100;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./bidder <bid_value> <total_work_ms>\n";
        std::cerr << "  bid_value      credits you're willing to spend per round\n";
        std::cerr << "  total_work_ms  simulated work this job needs before it's done\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << "\n";
        return 1;
    }

    struct sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORT);
    servAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int bidValue = std::atoi(argv[1]);
    int remainingWork = std::atoi(argv[2]);
    pid_t myPid = getpid();

    std::cout << "Bidder [" << myPid << "] starting: bid=" << bidValue
              << ", work=" << remainingWork << "ms\n";

    while (remainingWork > 0) {
        BidMessage msg{static_cast<int32_t>(myPid), bidValue, remainingWork};
        uint8_t buffer[BID_MESSAGE_WIRE_SIZE];
        serializeBid(msg, buffer);
        sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&servAddr, sizeof(servAddr));
        std::cout << "[" << myPid << "] bid " << bidValue
                  << " (work left: " << remainingWork << "ms)\n";

        // This loop only makes progress while the OS actually schedules
        // us. When the auctioneer sends SIGSTOP, the whole process -
        // including this loop - freezes at the kernel level, so we don't
        // need to check any flag ourselves to know whether we're
        // "allowed" to run right now.
        int elapsed = 0;
        while (elapsed < REBID_INTERVAL_MS && remainingWork > 0) {
            usleep(TICK_MS * 1000);
            elapsed += TICK_MS;
            remainingWork = std::max(0, remainingWork - TICK_MS);
        }
        std::cout << "[" << myPid << "] ran this round, " << remainingWork << "ms left\n";
    }

    std::cout << "[" << myPid << "] finished all work. Exiting.\n";
    close(sock);
    return 0;
}
