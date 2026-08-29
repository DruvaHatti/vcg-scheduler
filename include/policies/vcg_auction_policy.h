#ifndef VCG_AUCTION_POLICY_H
#define VCG_AUCTION_POLICY_H

#include <algorithm>
#include "../scheduler_policy.h"

// VCG (Vickrey-Clarke-Groves) second-price auction, adapted for CPU
// scheduling, with an aging-based starvation guard.
//
// This replaces the original project's "Robin Hood" mechanism, which
// injected a magic 50 credits whenever the VCG price happened to be
// zero. That patch only fired in one specific condition and wasn't
// derived from anything - it was a band-aid, not a mechanism. Two
// changes make starvation prevention principled instead of accidental:
//
//  1. UNIVERSAL ROUND INCOME. Every active job receives a fixed income
//     every round, regardless of the auction's outcome. This is a
//     subsidy paid OUTSIDE the auction, so it never distorts what
//     anyone should bid - it just guarantees every job's balance grows
//     over time even if it never wins, so it can eventually afford to
//     compete. Unlike the original hack, this runs unconditionally, not
//     only when the tax happened to hit exactly zero.
//
//  2. AGING-ADJUSTED SELECTION. The job that runs is chosen by an
//     *effective* priority: costBid + waitingRounds * AGING_RATE - the
//     same aging technique classic priority schedulers use to bound
//     worst-case wait time. A job that keeps losing eventually
//     outranks everyone regardless of its raw bid.
//
// Critically, the amount actually CHARGED to the winner is still the
// second-highest raw cost-bid, with the aging bonus excluded. The aging
// bonus is a scheduling nudge, not real economic value; if it leaked
// into the price, "bid your true value" would stop being each job's
// best strategy, which is the entire reason to use VCG pricing here.
class VcgAuctionPolicy : public SchedulerPolicy {
public:
    // Deliberately modest relative to typical bids (see benchmark.cpp's
    // workload, bids 10-95): if the starting balance and income are huge
    // compared to what anyone bids, the min(bid, balance) cap never
    // actually binds, and this degenerates into plain Priority+Aging -
    // the budget constraint has to be able to bite for VCG's economics
    // to mean anything.
    static constexpr int STARTING_BALANCE = 120;
    static constexpr int ROUND_INCOME = 15;
    static constexpr int AGING_RATE = 5;

    int lastPriceCharged = 0;

    Job* pickNext(std::vector<Job*>& activeJobs) override {
        for (auto* j : activeJobs) j->balance += ROUND_INCOME;

        Job* winner = nullptr;
        int highestEffective = -1;
        for (auto* j : activeJobs) {
            int costBid = std::min(j->bid_value, j->balance);
            int effective = costBid + j->waiting_rounds * AGING_RATE;
            if (effective > highestEffective) {
                highestEffective = effective;
                winner = j;
            }
        }
        return winner;
    }

    void onScheduled(Job& winner, std::vector<Job*>& activeJobs) override {
        int secondCostBid = 0;
        for (auto* j : activeJobs) {
            if (j->process_id == winner.process_id) continue;
            int costBid = std::min(j->bid_value, j->balance);
            secondCostBid = std::max(secondCostBid, costBid);
            j->waiting_rounds++;
        }
        lastPriceCharged = secondCostBid;
        winner.balance -= secondCostBid;
        winner.waiting_rounds = 0;
    }

    const char* name() const override { return "VCG Auction (aged)"; }
};

#endif
