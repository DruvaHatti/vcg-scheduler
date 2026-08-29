#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <sys/select.h>

#include "../include/protocol.h"
#include "../include/job.h"
#include "../include/scheduler_policy.h"
#include "../include/policies/vcg_auction_policy.h"

namespace {

volatile sig_atomic_t g_shutdownRequested = 0;
void handleSigint(int) { g_shutdownRequested = 1; }

// Sending signal 0 delivers nothing but still runs the kernel's
// existence/permission check - the standard portable way to test
// "is this PID still alive" without actually signalling it. We use this
// instead of SIGCHLD because bidder processes are launched independently
// by the user, not fork()'d by the auctioneer, so they are never our
// children and we'd never receive SIGCHLD for them.
bool isProcessAlive(pid_t pid) {
    return kill(pid, 0) == 0;
}

constexpr int BIDDING_WINDOW_SECONDS = 5;
constexpr int PORT = 8080;

} // namespace

int main() {
    signal(SIGINT, handleSigint);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << "\n";
        return 1;
    }

    struct sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORT);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        std::cerr << "Bind failed on port " << PORT << ": " << strerror(errno) << "\n";
        close(sock);
        return 1;
    }

    std::cout << "Auctioneer active on port " << PORT << ". Press Ctrl+C to stop.\n";

    VcgAuctionPolicy policy;
    std::vector<Job> jobs;
    pid_t currentlyRunningPid = -1;
    int round = 1;

    while (!g_shutdownRequested) {
        std::cout << "\n===================================\n";
        std::cout << "Round " << round << " - bidding window open for "
                  << BIDDING_WINDOW_SECONDS << "s...\n";

        struct timeval tv{BIDDING_WINDOW_SECONDS, 0};
        fd_set readfds;

        while (!g_shutdownRequested) {
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            int activity = select(sock + 1, &readfds, nullptr, nullptr, &tv);
            if (activity < 0) {
                if (errno == EINTR) break; // interrupted by SIGINT
                std::cerr << "select() error: " << strerror(errno) << "\n";
                break;
            }
            if (activity == 0) {
                std::cout << "--- Bidding window closed ---\n";
                break;
            }
            if (FD_ISSET(sock, &readfds)) {
                uint8_t buffer[BID_MESSAGE_WIRE_SIZE];
                struct sockaddr_in clientAddr{};
                socklen_t addrLen = sizeof(clientAddr);

                ssize_t received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                             (struct sockaddr*)&clientAddr, &addrLen);
                if (received != static_cast<ssize_t>(BID_MESSAGE_WIRE_SIZE)) {
                    std::cerr << "Dropped malformed packet (" << received
                              << " bytes, expected " << BID_MESSAGE_WIRE_SIZE << ")\n";
                    continue;
                }

                BidMessage incoming = deserializeBid(buffer);

                auto it = std::find_if(jobs.begin(), jobs.end(), [&](const Job& j) {
                    return j.process_id == incoming.process_id;
                });

                if (it == jobs.end()) {
                    Job newJob;
                    newJob.process_id = incoming.process_id;
                    newJob.bid_value = incoming.bid_value;
                    newJob.remaining_work_ms = incoming.remaining_work_ms;
                    newJob.balance = VcgAuctionPolicy::STARTING_BALANCE;
                    newJob.round_arrived = round;
                    jobs.push_back(newJob);
                    std::cout << "New job PID " << incoming.process_id
                              << " (bid=" << incoming.bid_value
                              << ", work=" << incoming.remaining_work_ms << "ms)\n";
                    if (kill(incoming.process_id, SIGSTOP) != 0) {
                        std::cerr << "Warning: could not pause PID " << incoming.process_id
                                  << ": " << strerror(errno) << "\n";
                    }
                } else {
                    // Existing job re-bidding: the bidder is the
                    // authority on its own remaining work (it's the one
                    // actually executing), so we just trust its report.
                    it->bid_value = incoming.bid_value;
                    it->remaining_work_ms = incoming.remaining_work_ms;
                }
            }
        }

        // Drop jobs whose process has died or exited since we last saw
        // it (e.g. finished all its work and returned).
        jobs.erase(std::remove_if(jobs.begin(), jobs.end(), [](const Job& j) {
            return j.finished || !isProcessAlive(j.process_id);
        }), jobs.end());

        std::cout << "Active jobs: " << jobs.size() << "\n";
        if (jobs.empty()) {
            round++;
            continue;
        }

        std::vector<Job*> activePtrs;
        activePtrs.reserve(jobs.size());
        for (auto& j : jobs) activePtrs.push_back(&j);

        Job* winner = policy.pickNext(activePtrs);
        if (!winner) {
            round++;
            continue;
        }
        policy.onScheduled(*winner, activePtrs);

        std::cout << "\n=== Round " << round << " result ===\n";
        std::cout << "Winner PID " << winner->process_id
                  << " | charged=" << policy.lastPriceCharged
                  << " | balance=" << winner->balance
                  << " | remaining_work=" << winner->remaining_work_ms << "ms\n";

        if (currentlyRunningPid != -1 && currentlyRunningPid != winner->process_id) {
            std::cout << "[CONTEXT SWITCH] Pausing previous winner PID "
                      << currentlyRunningPid << "\n";
            if (isProcessAlive(currentlyRunningPid)) kill(currentlyRunningPid, SIGSTOP);
        }
        if (kill(winner->process_id, SIGCONT) != 0) {
            std::cerr << "Failed to resume PID " << winner->process_id
                      << ": " << strerror(errno) << "\n";
        } else {
            currentlyRunningPid = winner->process_id;
        }
        winner->rounds_run++;

        round++;
    }

    std::cout << "\nShutting down - resuming any paused processes so none are left frozen...\n";
    for (auto& j : jobs) {
        if (isProcessAlive(j.process_id)) kill(j.process_id, SIGCONT);
    }
    close(sock);
    return 0;
}
