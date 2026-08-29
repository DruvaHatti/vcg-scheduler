#ifndef SJF_POLICY_H
#define SJF_POLICY_H

#include "../scheduler_policy.h"

// Shortest-Job-First (evaluated every round, so effectively SRTF -
// Shortest Remaining Time First - since a newly arrived shorter job can
// preempt the current pick). Minimizes average waiting time in theory,
// but a steady stream of short jobs can starve a long one indefinitely.
class SjfPolicy : public SchedulerPolicy {
public:
    Job* pickNext(std::vector<Job*>& activeJobs) override {
        Job* shortest = nullptr;
        for (auto* j : activeJobs) {
            if (!shortest || j->remaining_work_ms < shortest->remaining_work_ms) shortest = j;
        }
        return shortest;
    }

    const char* name() const override { return "SJF"; }
};

#endif
