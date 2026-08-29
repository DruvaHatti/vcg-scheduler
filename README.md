# VCG Scheduler

A CPU scheduler simulation, written in C++, that allocates simulated CPU
time using the **Vickrey-Clarke-Groves (VCG)** second-price auction
mechanism, alongside classic OS scheduling policies for direct
comparison.

Separate "bidder" processes each bid for CPU time over UDP; a central
"auctioneer" process resolves each round's auction and uses real POSIX
signals (`SIGSTOP` / `SIGCONT`) to actually pause and resume the losing
and winning processes on the host.

> **Scope note:** this runs multiple communicating processes over UDP,
> but `SIGSTOP`/`SIGCONT` only work within a single machine's kernel -
> you cannot signal a process on a different host. This is a multi-process,
> network-IPC scheduler simulation on one machine, not a distributed
> system, and the project doesn't claim otherwise.

## Why VCG for CPU scheduling?

In a VCG auction, the highest bidder wins, but pays the *second*-highest
bid, not their own. That structure makes truthfully reporting your real
valuation each round the dominant strategy for every bidder - there's no
incentive to shade your bid up or down. Applying that to CPU scheduling
means processes are incentivized to bid what the CPU time is genuinely
worth to them, and the scheduler's job becomes finding an economically
efficient allocation rather than following a fixed heuristic.

## Design decisions

**Starvation prevention is principled, not a patch.** An earlier version
of this project injected a flat 50 credits whenever the auction's price
happened to hit exactly zero - a special case that wasn't derived from
anything and only fired under one specific condition. The current
mechanism (`include/policies/vcg_auction_policy.h`) instead:
1. Pays every active job a fixed income every round, independent of the
   auction's outcome - a subsidy *outside* the auction, so it never
   distorts what anyone should bid.
2. Selects the winner by an *aged* effective priority (`cost-bid +
   waiting_rounds * AGING_RATE`), the same aging technique classic
   priority schedulers use to bound worst-case wait time.
3. Still *charges* the winner only the second-highest raw bid, with the
   aging bonus excluded - the aging term is a scheduling nudge, not real
   economic value, so it must not leak into the price or "bid your true
   value" stops being the best strategy.

**Wire protocol is byte-order safe.** Bids are serialized into a fixed
12-byte buffer using explicit network byte order
(`include/protocol.h`), rather than sending a raw C++ struct over the
socket. Raw struct transmission depends on padding, alignment, and
`pid_t` size all matching on both ends, which isn't guaranteed across
compilers or architectures.

**The live auctioneer trusts bidder-reported progress.** Each bidder is
the only process that actually knows how much of its own simulated work
is left (it's the one executing it, and it's the one that gets frozen
mid-execution by `SIGSTOP`). The auctioneer treats each bid packet's
`remaining_work_ms` as authoritative rather than keeping an independent,
possibly-inconsistent copy. Job completion/removal is detected by
checking whether the process is still alive (`kill(pid, 0)`), not by the
auctioneer's own countdown.

**Policies are pluggable.** `SchedulerPolicy` (`include/scheduler_policy.h`)
is a small interface implemented by five policies - FCFS, SJF, Round
Robin, Priority+Aging, and the VCG auction - so the same code can run
live over the network (`auctioneer.cpp`, currently wired to VCG) or
offline in a deterministic benchmark (`benchmark.cpp`, all five).

## Project layout

```
include/
  protocol.h              wire-safe Bid message (de)serialization
  job.h                   shared Job/process state
  scheduler_policy.h       policy interface
  policies/
    fcfs_policy.h
    sjf_policy.h
    round_robin_policy.h
    priority_aging_policy.h
    vcg_auction_policy.h
src/
  auctioneer.cpp           live networked scheduler (UDP + signals)
  bidder.cpp               client process that bids and simulates work
  benchmark.cpp            offline comparison of all five policies
tests/
  test_policies.cpp        unit tests for each policy's core behavior
```

## Building

Requires `g++` (C++17) and `make` on Linux/WSL.

```bash
make            # builds auctioneer, bidder, benchmark
make test       # builds and runs the unit test suite
make clean
```

## Running the live scheduler

In one terminal:
```bash
./auctioneer
```

In separate terminals, start however many bidders you like:
```bash
./bidder <bid_value> <total_work_ms>
# e.g.
./bidder 80 800
./bidder 50 600
```
Each bidder registers with the auctioneer, gets `SIGSTOP`'d, and waits
its turn. The auctioneer resolves an auction every 5 seconds, resumes the
winner with `SIGCONT`, and pauses whoever was previously running. Press
`Ctrl+C` on the auctioneer to shut down cleanly (it resumes any paused
processes before exiting).

## Running the benchmark

`benchmark.cpp` runs an identical fixed 8-job workload through all five
policies with no networking involved, so the results are deterministic
and reproducible:

```
$ ./benchmark
Benchmarking scheduling policies on a fixed 8-job workload
(job 3 has low priority to test starvation resistance;
 job 6 arrives late with little work to test responsiveness)

FCFS                 | total_rounds=43    | avg_wait=18.62   | avg_turnaround=23.00   | max_wait=30
SJF                  | total_rounds=43    | avg_wait=11.38   | avg_turnaround=15.75   | max_wait=34
Round Robin          | total_rounds=43    | avg_wait=21.00   | avg_turnaround=25.38   | max_wait=32
Priority + Aging     | total_rounds=43    | avg_wait=18.12   | avg_turnaround=22.50   | max_wait=34
VCG Auction (aged)   | total_rounds=43    | avg_wait=18.38   | avg_turnaround=22.75   | max_wait=34
```

Takeaways from this workload: SJF gets the best average wait time, as
expected, at the cost of a higher `max_wait` for the one long job it
keeps deprioritizing. VCG (aged) tracks Priority+Aging closely here
because the workload's bids stay well within everyone's balance most of
the time - the two mechanisms only diverge meaningfully when a job's
bid actually exceeds what it can afford, which is a deliberate,
honestly-reported result, not a cherry-picked one.

## Testing

`tests/test_policies.cpp` is a small hand-rolled test harness (no
external framework, since ~7 assertions didn't justify one) that checks
each policy's defining behavior in isolation - e.g. that VCG charges the
second-highest bid rather than the winner's own, and that Priority+Aging
eventually promotes a permanently-outbid job.

## Known limitations

- This is a round-based, discretized simulation (fixed-length bidding
  windows), not a continuous-time scheduler.
- `SIGSTOP`/`SIGCONT` require all processes to be on the same host and
  visible to the same kernel - see the scope note above.
- The live auctioneer trusts bidders to report their own progress
  honestly; there's no verification. Fine for a simulation among
  cooperating processes, not something a real scheduler could assume.
