#ifndef FCFS_POLICY_H
#define FCFS_POLICY_H

#include "../scheduler_policy.h"

// First-Come-First-Served: always run whichever active job arrived
// earliest, regardless of how much work it needs or what it bids.
// Simple, but a single long job at the front starves everyone behind it
// (the classic "convoy effect") - useful as the naive baseline in the
// benchmark comparison.
class FcfsPolicy : public SchedulerPolicy {
public:
    Job* pickNext(std::vector<Job*>& activeJobs) override {
        Job* earliest = nullptr;
        for (auto* j : activeJobs) {
            if (!earliest || j->round_arrived < earliest->round_arrived) earliest = j;
        }
        return earliest;
    }

    const char* name() const override { return "FCFS"; }
};

#endif
