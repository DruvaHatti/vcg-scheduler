#ifndef SCHEDULER_POLICY_H
#define SCHEDULER_POLICY_H

#include <vector>
#include "job.h"

// A SchedulerPolicy decides which job runs next given the set of
// currently runnable jobs. Every policy operates on Job* (pointers into
// the caller's real Job objects) rather than copies - that way there's
// a single source of truth for each job's state, and no risk of a
// "working copy" drifting out of sync with the real one.
//
// This interface is what lets auctioneer.cpp (the live, networked
// scheduler) and benchmark.cpp (the offline comparison harness) share
// the exact same policy implementations.
class SchedulerPolicy {
public:
    virtual ~SchedulerPolicy() = default;

    // Pick the next job to run. Returns nullptr if activeJobs is empty.
    virtual Job* pickNext(std::vector<Job*>& activeJobs) = 0;

    // Called once pickNext has chosen a winner, so the policy can update
    // its own bookkeeping (VCG pricing, aging counters, round-robin
    // rotation state, etc.) before the caller acts on the result.
    virtual void onScheduled(Job& /*winner*/, std::vector<Job*>& /*activeJobs*/) {}

    virtual const char* name() const = 0;
};

#endif
