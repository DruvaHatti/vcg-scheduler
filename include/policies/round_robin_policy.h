#ifndef ROUND_ROBIN_POLICY_H
#define ROUND_ROBIN_POLICY_H

#include "../scheduler_policy.h"

// Round Robin: rotate through active jobs in order, one per round.
// We track the PID (not an index) of whoever ran last, so rotation
// stays correct even when jobs join or finish between rounds and the
// active set's size/order changes - an index-based cursor would silently
// skip or repeat jobs whenever the composition shifts.
class RoundRobinPolicy : public SchedulerPolicy {
    pid_t lastServedPid = -1;

public:
    Job* pickNext(std::vector<Job*>& activeJobs) override {
        if (activeJobs.empty()) return nullptr;

        size_t startIdx = 0;
        for (size_t i = 0; i < activeJobs.size(); ++i) {
            if (activeJobs[i]->process_id == lastServedPid) {
                startIdx = i + 1;
                break;
            }
        }
        return activeJobs[startIdx % activeJobs.size()];
    }

    void onScheduled(Job& winner, std::vector<Job*>&) override {
        lastServedPid = winner.process_id;
    }

    const char* name() const override { return "Round Robin"; }
};

#endif
