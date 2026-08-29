#ifndef JOB_H
#define JOB_H

#include <sys/types.h>

// A Job is one process competing for CPU time. It's shared by every
// scheduling policy (FCFS, SJF, Round Robin, Priority+Aging, VCG) so
// they can all be benchmarked on identical state.
struct Job {
    pid_t process_id = -1;

    int bid_value = 0;          // stated bid (VCG) or static priority (Priority+Aging)
    int remaining_work_ms = 0;  // simulated burst time remaining
    int balance = 0;            // virtual currency; only meaningful for VCG

    int round_arrived = 0;
    int waiting_rounds = 0;     // consecutive rounds since this job last ran (drives aging)
    int total_wait_rounds = 0;  // cumulative rounds spent waiting, for reporting only
    int rounds_run = 0;
    int round_finished = -1;    // -1 = not finished yet

    bool finished = false;
};

#endif
