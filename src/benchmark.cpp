#include <iostream>
#include <vector>
#include <iomanip>
#include <string>
#include <algorithm>

#include "../include/job.h"
#include "../include/scheduler_policy.h"
#include "../include/policies/fcfs_policy.h"
#include "../include/policies/sjf_policy.h"
#include "../include/policies/round_robin_policy.h"
#include "../include/policies/priority_aging_policy.h"
#include "../include/policies/vcg_auction_policy.h"

// Offline, deterministic comparison of every scheduling policy on an
// identical fixed workload. This exists so the VCG auction's fairness
// claim isn't just asserted - it's measured, the same way you'd back up
// a scheduling-algorithm claim in an OS course project.

namespace {

constexpr int WORK_PER_ROUND = 100;
constexpr int MAX_ROUNDS = 500;

// A fixed, hand-picked workload so results are reproducible.
// Job 3 has a deliberately low priority/bid despite arriving early, to
// test starvation resistance. Job 6 arrives late needing very little
// work, to test responsiveness to short, urgent jobs.
struct WorkloadEntry {
    pid_t id;
    int arrival;
    int workNeeded;
    int priorityOrBid;
};

std::vector<WorkloadEntry> workload() {
    return {
        {1, 0, 800, 40},
        {2, 0, 600, 90},
        {3, 0, 900, 10},  // low priority - starvation risk
        {4, 1, 400, 15},
        {5, 2, 300, 20},
        {6, 5, 100, 95},  // arrives late, short, high urgency
        {7, 6, 700, 30},
        {8, 8, 500, 60},
    };
}

std::vector<Job> makeJobs() {
    std::vector<Job> jobs;
    for (const auto& w : workload()) {
        Job j;
        j.process_id = w.id;
        j.bid_value = w.priorityOrBid;
        j.remaining_work_ms = w.workNeeded;
        j.balance = VcgAuctionPolicy::STARTING_BALANCE;
        j.round_arrived = w.arrival;
        jobs.push_back(j);
    }
    return jobs;
}

struct AggregateResult {
    std::string policyName;
    int roundsToComplete = 0;
    double avgWaitingRounds = 0;
    double avgTurnaroundRounds = 0;
    int maxWaitingRounds = 0;
};

AggregateResult runSimulation(SchedulerPolicy& policy) {
    std::vector<Job> jobs = makeJobs();
    int round = 0;
    int completed = 0;

    while (completed < static_cast<int>(jobs.size()) && round < MAX_ROUNDS) {
        std::vector<Job*> active;
        for (auto& j : jobs) {
            if (!j.finished && j.round_arrived <= round) active.push_back(&j);
        }

        if (active.empty()) {
            round++;
            continue;
        }

        Job* winner = policy.pickNext(active);
        if (!winner) {
            round++;
            continue;
        }
        policy.onScheduled(*winner, active);

        winner->remaining_work_ms -= WORK_PER_ROUND;
        winner->rounds_run++;
        if (winner->remaining_work_ms <= 0) {
            winner->finished = true;
            winner->round_finished = round;
            completed++;
        }

        for (auto* j : active) {
            if (j != winner) j->total_wait_rounds++;
        }

        round++;
    }

    AggregateResult result;
    result.policyName = policy.name();
    result.roundsToComplete = round;

    double waitSum = 0, turnaroundSum = 0;
    for (const auto& j : jobs) {
        waitSum += j.total_wait_rounds;
        result.maxWaitingRounds = std::max(result.maxWaitingRounds, j.total_wait_rounds);
        int finishRound = j.finished ? j.round_finished : round;
        turnaroundSum += (finishRound - j.round_arrived);
    }
    result.avgWaitingRounds = waitSum / jobs.size();
    result.avgTurnaroundRounds = turnaroundSum / jobs.size();
    return result;
}

void printResult(const AggregateResult& r) {
    std::cout << std::left << std::setw(20) << r.policyName
              << " | total_rounds=" << std::setw(5) << r.roundsToComplete
              << " | avg_wait=" << std::setw(7) << std::fixed << std::setprecision(2)
              << r.avgWaitingRounds
              << " | avg_turnaround=" << std::setw(7) << r.avgTurnaroundRounds
              << " | max_wait=" << r.maxWaitingRounds
              << std::endl;
}

} // namespace

int main() {
    std::cout << "Benchmarking scheduling policies on a fixed 8-job workload\n";
    std::cout << "(job 3 has low priority to test starvation resistance;\n";
    std::cout << " job 6 arrives late with little work to test responsiveness)\n\n";

    FcfsPolicy fcfs;
    SjfPolicy sjf;
    RoundRobinPolicy rr;
    PriorityAgingPolicy pa;
    VcgAuctionPolicy vcg;

    std::vector<SchedulerPolicy*> policies = {&fcfs, &sjf, &rr, &pa, &vcg};

    for (auto* p : policies) {
        printResult(runSimulation(*p));
    }

    return 0;
}
