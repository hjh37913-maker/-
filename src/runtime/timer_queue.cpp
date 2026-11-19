#include "runtime/timer_queue.h"
#include <limits>
#include <utility>
namespace nightfall::runtime {
ScheduleResult TimerQueue::schedule(std::uint64_t delay, std::uint64_t tag) {
    if (events_.size()>=capacity_) return {ScheduleStatus::capacity_reached,0};
    if (delay>std::numeric_limits<std::uint64_t>::max()-now_) return {ScheduleStatus::time_overflow,0};
    if (!next_id_) return {ScheduleStatus::id_exhausted,0};
    const auto id=next_id_;
    events_.emplace(std::make_pair(now_+delay,id),tag); // if allocation throws, ID is not consumed
    ++next_id_; // unsigned wrap to zero permanently signals exhaustion
    return {ScheduleStatus::ok,id};
}
bool TimerQueue::cancel(TimerId id) noexcept {
    if (!id) return false;
    for (auto it=events_.begin();it!=events_.end();++it) {
        if (it->first.second==id) { events_.erase(it); return true; }
    }
    return false;
}
AdvanceResult TimerQueue::advance_to(std::uint64_t target, std::size_t budget) {
    if (target<now_) return {AdvanceStatus::time_reversed,{},false};
    std::size_t count=0;
    for (auto it=events_.begin();it!=events_.end() && it->first.first<=target && count<budget;++it) ++count;
    AdvanceResult result{AdvanceStatus::ok,{},false};
    result.events.reserve(count); // Allocation failure leaves queue and time unchanged.
    now_=target;
    while (result.events.size()<count) {
        const auto it=events_.begin();
        result.events.push_back({it->first.second,it->first.first,it->second});
        events_.erase(it);
    }
    result.due_remaining=!events_.empty() && events_.begin()->first.first<=now_;
    return result;
}
AdvanceResult TimerQueue::advance_by(std::uint64_t delta, std::size_t budget) {
    if (delta>std::numeric_limits<std::uint64_t>::max()-now_) return {AdvanceStatus::time_overflow,{},false};
    return advance_to(now_+delta,budget);
}
}
