#include <iostream>
#include <vector>
#include <string>

#include "../include/job.h"
#include "../include/policies/fcfs_policy.h"
#include "../include/policies/sjf_policy.h"
#include "../include/policies/round_robin_policy.h"
#include "../include/policies/priority_aging_policy.h"
#include "../include/policies/vcg_auction_policy.h"

// A small hand-rolled test harness. For ~6 assertions, pulling in a full
// framework like Catch2/GoogleTest is more dependency than the project
// needs to explain or manage; a tiny check() helper does the job.

namespace {
int g_passed = 0;
int g_failed = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        g_passed++;
    } else {
        g_failed++;
        std::cerr << "FAILED: " << description << "\n";
    }
}

Job makeJob(pid_t id, int bid, int work, int arrival = 0, int balance = 1000) {
    Job j;
    j.process_id = id;
    j.bid_value = bid;
    j.remaining_work_ms = work;
    j.round_arrived = arrival;
    j.balance = balance;
    return j;
}
} // namespace

void testFcfsPicksEarliestArrival() {
    Job a = makeJob(1, 10, 100, /*arrival=*/2);
    Job b = makeJob(2, 50, 100, /*arrival=*/0);
    std::vector<Job*> jobs = {&a, &b};
    FcfsPolicy policy;
    Job* winner = policy.pickNext(jobs);
    check(winner->process_id == 2, "FCFS should pick the job that arrived first, not the highest bid");
}

void testSjfPicksShortestRemainingWork() {
    Job a = makeJob(1, 10, 500);
    Job b = makeJob(2, 10, 50);
    std::vector<Job*> jobs = {&a, &b};
    SjfPolicy policy;
    Job* winner = policy.pickNext(jobs);
    check(winner->process_id == 2, "SJF should pick the job with the least remaining work");
}

void testRoundRobinRotatesFairly() {
    Job a = makeJob(1, 0, 1000);
    Job b = makeJob(2, 0, 1000);
    Job c = makeJob(3, 0, 1000);
    std::vector<Job*> jobs = {&a, &b, &c};
    RoundRobinPolicy policy;

    Job* first = policy.pickNext(jobs);
    policy.onScheduled(*first, jobs);
    Job* second = policy.pickNext(jobs);
    check(first->process_id != second->process_id,
          "Round Robin must not pick the same job twice in a row when others are runnable");
}

void testPriorityAgingEventuallyPromotesLowPriorityJob() {
    Job low = makeJob(1, /*bid=*/1, 1000);
    Job high = makeJob(2, /*bid=*/1000, 1000); // always outbids "low" on raw priority
    std::vector<Job*> jobs = {&low, &high};
    PriorityAgingPolicy policy;

    bool lowEventuallyWon = false;
    for (int round = 0; round < 500; ++round) {
        Job* winner = policy.pickNext(jobs);
        policy.onScheduled(*winner, jobs);
        if (winner->process_id == low.process_id) {
            lowEventuallyWon = true;
            break;
        }
    }
    check(lowEventuallyWon, "Priority+Aging must prevent permanent starvation of a low-priority job");
}

void testVcgChargesSecondPriceNotFirstPrice() {
    Job a = makeJob(1, /*bid=*/300, 1000, 0, /*balance=*/1000);
    Job b = makeJob(2, /*bid=*/150, 1000, 0, /*balance=*/1000);
    std::vector<Job*> jobs = {&a, &b};
    VcgAuctionPolicy policy;

    Job* winner = policy.pickNext(jobs); // this also applies round income as a side effect
    check(winner->process_id == a.process_id, "VCG should select the highest cost-bid as winner");

    int balanceBeforeCharge = winner->balance;
    policy.onScheduled(*winner, jobs);
    int charged = balanceBeforeCharge - winner->balance;
    check(charged == 150, "VCG must charge the second-highest bid, not the winner's own bid");
}

void testVcgAppliesRoundIncomeToEveryone() {
    Job a = makeJob(1, 300, 1000, 0, 1000);
    Job b = makeJob(2, 150, 1000, 0, 500);
    std::vector<Job*> jobs = {&a, &b};
    VcgAuctionPolicy policy;

    int loserBalanceBefore = b.balance;
    policy.pickNext(jobs); // applies round income as a side effect, regardless of who wins
    check(b.balance == loserBalanceBefore + VcgAuctionPolicy::ROUND_INCOME,
          "Every active job, including losers, must receive the per-round income");
}

int main() {
    testFcfsPicksEarliestArrival();
    testSjfPicksShortestRemainingWork();
    testRoundRobinRotatesFairly();
    testPriorityAgingEventuallyPromotesLowPriorityJob();
    testVcgChargesSecondPriceNotFirstPrice();
    testVcgAppliesRoundIncomeToEveryone();

    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
