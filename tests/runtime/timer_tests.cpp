#include "runtime/timer_queue.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
using namespace nightfall::runtime;
namespace {
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
bool order(const TimerEvent& a,const TimerEvent& b) {
    return a.deadline<b.deadline || (a.deadline==b.deadline && a.id<b.id);
}
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
}
int main() {
    { TimerQueue q(3,100); auto a=q.schedule(5,1); auto b=q.schedule(5,2); auto c=q.schedule(0,3);
      CHECK(a.status==ScheduleStatus::ok && b.status==ScheduleStatus::ok && c.status==ScheduleStatus::ok);
      CHECK(q.schedule(0,4).status==ScheduleStatus::capacity_reached);
      auto zero=q.advance_to(105,0); CHECK(zero.events.empty() && zero.due_remaining && q.now()==105);
      auto first=q.advance_to(105,1); CHECK(first.events.size()==1 && first.events[0].id==c.id && first.due_remaining);
      CHECK(q.cancel(a.id)); CHECK(!q.cancel(a.id)); CHECK(!q.cancel(0));
      auto last=q.advance_by(0,4); CHECK(last.events.size()==1 && last.events[0].id==b.id && !last.due_remaining);
      CHECK(q.advance_to(104,10).status==AdvanceStatus::time_reversed && q.now()==105);
      CHECK(q.advance_to(106,10).events.empty() && q.pending()==0);
    }
    { TimerQueue q(1,UINT64_MAX-1); CHECK(q.schedule(2,0).status==ScheduleStatus::time_overflow);
      auto a=q.schedule(1,4); CHECK(a.status==ScheduleStatus::ok);
      CHECK(q.advance_by(2,1).status==AdvanceStatus::time_overflow && q.now()==UINT64_MAX-1 && q.pending()==1);
      auto r=q.advance_by(1,1); CHECK(r.events.size()==1 && r.events[0].deadline==UINT64_MAX);
      CHECK(q.schedule(0,6).status==ScheduleStatus::ok); CHECK(q.schedule(1,7).status==ScheduleStatus::capacity_reached);
    }
    { TimerQueue q(0); CHECK(q.schedule(0,0).status==ScheduleStatus::capacity_reached); }
    std::uint64_t accepted=0,rejected=0,cancelled=0,advanced=0;
    // Reference model uses a sorted vector, not the production map.
    for (std::uint64_t seed=1;seed<=100;++seed) {
        TimerQueue q(4); std::vector<TimerEvent> model; std::uint64_t now=0,id=1;
        std::uint64_t rng=0x54494d455f515545ULL ^ (seed*0x9e3779b97f4a7c15ULL);
        for (unsigned step=0;step<1000;++step) {
            const auto operation=next(rng)%3;
            if (operation==0) {
                const auto delay=next(rng)%25, tag=next(rng);
                auto result=q.schedule(delay,tag);
                if (model.size()==4) { CHECK(result.status==ScheduleStatus::capacity_reached); ++rejected; }
                else { CHECK(result.status==ScheduleStatus::ok && result.id==id);
                    model.push_back({id++,now+delay,tag}); ++accepted; }
            } else if (operation==1) {
                auto chosen=next(rng)%2 && !model.empty() ? model[next(rng)%model.size()].id : next(rng);
                auto it=std::find_if(model.begin(),model.end(),[chosen](const TimerEvent& e){return e.id==chosen;});
                const bool expected=it!=model.end(); CHECK(q.cancel(chosen)==expected);
                if (expected) { model.erase(it); ++cancelled; }
            } else {
                const auto delta=next(rng)%8, budget=static_cast<std::size_t>(next(rng)%5);
                now+=delta; auto result=q.advance_to(now,budget); ++advanced;
                std::sort(model.begin(),model.end(),order);
                std::vector<TimerEvent> expected;
                while (!model.empty() && model.front().deadline<=now && expected.size()<budget) {
                    expected.push_back(model.front()); model.erase(model.begin());
                }
                CHECK(result.status==AdvanceStatus::ok && result.events==expected);
                CHECK(result.due_remaining==(!model.empty() && model.front().deadline<=now));
            }
            if (q.pending()!=model.size() || q.now()!=now) {
                std::cerr << "Mismatch seed=" << seed << " step=" << step << '\n'; return 1;
            }
        }
    }
    CHECK(accepted && rejected && cancelled && advanced);
    std::cout << "100 scenarios, 1000 operations each; scheduled=" << accepted << " full=" << rejected
              << " cancelled=" << cancelled << " advanced=" << advanced << '\n';
}
