#include "runtime/timer_queue.h"
#include <chrono>
#include <cstdint>
#include <iostream>
using namespace nightfall::runtime;
int main() {
    constexpr std::uint64_t rounds=10000, per_round=32;
    TimerQueue queue(per_round); std::uint64_t count=0;
    const auto start=std::chrono::steady_clock::now();
    for (std::uint64_t i=0;i<rounds;++i) {
        for (std::uint64_t j=0;j<per_round;++j)
            if (queue.schedule(j%8,j).status!=ScheduleStatus::ok) return 1;
        const auto r=queue.advance_by(8,per_round);
        if (r.status!=AdvanceStatus::ok || r.events.size()!=per_round || r.due_remaining) return 1;
        count+=r.events.size();
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Synthetic timer workload: " << count << " scheduled and delivered events; " << rounds << " batches\n"
              << "Elapsed ns: " << ns << '\n';
}
