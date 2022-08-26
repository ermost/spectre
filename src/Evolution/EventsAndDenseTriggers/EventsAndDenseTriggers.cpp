// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/EventsAndDenseTriggers/EventsAndDenseTriggers.hpp"

#include <algorithm>
#include <pup.h>
#include <utility>

namespace evolution {
void EventsAndDenseTriggers::TriggerRecord::pup(PUP::er& p) {
  p | next_check;
  p | trigger;
  p | events;
}

EventsAndDenseTriggers::EventsAndDenseTriggers(
    ConstructionType events_and_triggers) {
  events_and_triggers_.reserve(events_and_triggers.size());
  while (not events_and_triggers.empty()) {
    auto events_and_trigger =
        events_and_triggers.extract(events_and_triggers.begin());
    events_and_triggers_.push_back(
        TriggerRecord{std::numeric_limits<double>::signaling_NaN(),
                      std::move(events_and_trigger.key()),
                      std::move(events_and_trigger.mapped())});
  }
}

void EventsAndDenseTriggers::add_trigger_and_events(
    std::unique_ptr<DenseTrigger> trigger,
    std::vector<std::unique_ptr<Event>> events) {
  ASSERT(not initialized(), "Cannot add events after initialization");
  events_and_triggers_.reserve(events_and_triggers_.size() + 1);
  events_and_triggers_.push_back(
      TriggerRecord{std::numeric_limits<double>::signaling_NaN(),
                    std::move(trigger), std::move(events)});
}

void EventsAndDenseTriggers::pup(PUP::er& p) {
  p | events_and_triggers_;
  p | heap_size_;
  p | to_run_position_;
  p | processing_position_;
  p | event_to_check_;
  p | next_check_;
  p | next_check_after_;
}

bool EventsAndDenseTriggers::initialized() const {
  return heap_size_ != std::numeric_limits<size_t>::max();
}

void EventsAndDenseTriggers::populate_active_triggers() {
  ASSERT(not events_and_triggers_.empty(), "No triggers");
  ASSERT(heap_end() == events_and_triggers_.end(),
         "Triggers have not all been processed.");

  next_check_ = events_and_triggers_.front().next_check;
  while (heap_size_ > 0 and
         events_and_triggers_.front().next_check == next_check_) {
    std::pop_heap(events_and_triggers_.begin(), heap_end(), next_check_after_);
    --heap_size_;
  }
  to_run_position_ = heap_size_;
  processing_position_ = heap_size_;
}

void EventsAndDenseTriggers::reschedule_next_trigger(
    const double next_check_time, const bool time_runs_forward) {
  if (not evolution_greater<double>{time_runs_forward}(
          next_check_time, heap_end()->next_check)) {
    ERROR("Trigger at time " << heap_end()->next_check
          << " rescheduled itself for earlier time "
          << next_check_time);
  }
  heap_end()->next_check = next_check_time;
  ++heap_size_;
  std::push_heap(events_and_triggers_.begin(), heap_end(), next_check_after_);
}

EventsAndDenseTriggers::TriggerTimeAfter::TriggerTimeAfter(
    const bool time_runs_forward)
    : time_after_{time_runs_forward} {}

bool EventsAndDenseTriggers::TriggerTimeAfter::operator()(
    const TriggerRecord& a, const TriggerRecord& b) const {
  return time_after_(a.next_check, b.next_check);
}

double EventsAndDenseTriggers::TriggerTimeAfter::infinite_future() const {
  return time_after_.infinity();
}

void EventsAndDenseTriggers::TriggerTimeAfter::pup(PUP::er& p) {
  p | time_after_;
}
}  // namespace evolution
