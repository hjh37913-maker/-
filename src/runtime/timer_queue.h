#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace nightfall::runtime {
using TimerId = std::uint64_t;
struct TimerEvent {
    TimerId id;
    std::uint64_t deadline;
    std::uint64_t tag;
    bool operator==(const TimerEvent&) const = default;
};
enum class ScheduleStatus { ok, capacity_reached, time_overflow, id_exhausted };
struct ScheduleResult { ScheduleStatus status; TimerId id=0; };
enum class AdvanceStatus { ok, time_reversed, time_overflow };
struct AdvanceResult {
    AdvanceStatus status;
    std::vector<TimerEvent> events;
    bool due_remaining=false;
};
// A synthetic, manually advanced clock. Units are chosen by the caller.
// No wall-clock, callback execution, threading, or guest ABI semantics.
class TimerQueue {
public:
    explicit TimerQueue(std::size_t capacity, std::uint64_t initial_time=0) noexcept
        : capacity_(capacity), now_(initial_time) {}
    ScheduleResult schedule(std::uint64_t delay, std::uint64_t tag);
    bool cancel(TimerId id) noexcept;
    AdvanceResult advance_to(std::uint64_t target, std::size_t budget);
    AdvanceResult advance_by(std::uint64_t delta, std::size_t budget);
    std::uint64_t now() const noexcept { return now_; }
    std::size_t pending() const noexcept { return events_.size(); }
private:
    std::size_t capacity_;
    std::uint64_t now_;
    TimerId next_id_=1;
    // Unique monotonic IDs also provide stable ordering at equal deadlines.
    std::map<std::pair<std::uint64_t,TimerId>,std::uint64_t> events_;
};
}
