#ifndef PRIORITY_AGING_POLICY_H
#define PRIORITY_AGING_POLICY_H

#include "../scheduler_policy.h"

// Static priority scheduling with aging. A job's effective priority is
// its base priority (bid_value) plus a bonus that grows with every round
// it spends waiting. This is the classic OS technique for bounding
// worst-case wait time under priority scheduling: no matter how low a
// job's base priority is, its effective priority eventually exceeds any
// fixed competitor, guaranteeing it runs.
class PriorityAgingPolicy : public SchedulerPolicy {
public:
    static constexpr int AGING_RATE = 5; // priority points gained per round waited

    Job* pickNext(std::vector<Job*>& activeJobs) override {
        Job* best = nullptr;
        int bestEffective = -1;
        for (auto* j : activeJobs) {
            int effective = j->bid_value + j->waiting_rounds * AGING_RATE;
            if (effective > bestEffective) {
                bestEffective = effective;
                best = j;
            }
        }
        return best;
    }

    void onScheduled(Job& winner, std::vector<Job*>& activeJobs) override {
        for (auto* j : activeJobs) {
            if (j->process_id == winner.process_id) j->waiting_rounds = 0;
            else j->waiting_rounds++;
        }
    }

    const char* name() const override { return "Priority + Aging"; }
};

#endif
