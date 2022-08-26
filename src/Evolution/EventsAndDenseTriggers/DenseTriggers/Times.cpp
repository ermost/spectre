// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/EventsAndDenseTriggers/DenseTriggers/Times.hpp"

#include <optional>
#include <pup_stl.h>
#include <utility>

#include "Time/EvolutionOrdering.hpp"
#include "Time/TimeStepId.hpp"

namespace DenseTriggers {
Times::Times(std::unique_ptr<TimeSequence<double>> times)
    : times_(std::move(times)) {}

void Times::pup(PUP::er& p) {
  DenseTrigger::pup(p);
  p | times_;
}

std::optional<bool> Times::is_triggered_impl(const double time) const {
  const auto trigger_times = times_->times_near(time);

  return time == trigger_times[1];
}

std::optional<double> Times::next_check_time_impl(
    const TimeStepId& time_step_id, const double time) const {
  const evolution_less<double> before{time_step_id.time_runs_forward()};

  const auto trigger_times = times_->times_near(time);
  double next_time = before.infinity();
  for (const auto& trigger_time : trigger_times) {
    if (trigger_time.has_value() and before(time, *trigger_time) and
        before(*trigger_time, next_time)) {
      next_time = *trigger_time;
    }
  }

  return next_time;
}

PUP::able::PUP_ID Times::my_PUP_ID = 0;  // NOLINT
}  // namespace DenseTriggers
