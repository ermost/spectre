// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "ControlSystem/TimescaleTuner.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <ostream>
#include <pup.h>
#include <tuple>

#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/Gsl.hpp"

namespace {
struct TimescaleCreator {
  std::pair<DataVector, bool> operator()(const double timescale) {
    return std::make_pair(DataVector{1, timescale}, false);
  }

  std::pair<DataVector, bool> operator()(
      const std::vector<double>& input_timescales) {
    DataVector timescales{input_timescales.size()};
    for (size_t i = 0; i < input_timescales.size(); i++) {
      timescales[i] = input_timescales[i];
    }
    return std::make_pair(std::move(timescales), true);
  }
};
}  // namespace

TimescaleTuner::TimescaleTuner(
    const typename InitialTimescales::type& initial_timescale,
    const double max_timescale, const double min_timescale,
    const double decrease_timescale_threshold,
    const double increase_timescale_threshold, const double increase_factor,
    const double decrease_factor)
    : max_timescale_{max_timescale},
      min_timescale_{min_timescale},
      decrease_timescale_threshold_{decrease_timescale_threshold},
      increase_timescale_threshold_{increase_timescale_threshold},
      increase_factor_{increase_factor},
      decrease_factor_{decrease_factor} {
  std::tie(timescale_, timescales_have_been_set_) =
      std::visit(TimescaleCreator{}, initial_timescale);

  if (timescales_have_been_set_) {
    initial_timescale_ = min(timescale_);
  } else {
    initial_timescale_ = timescale_[0];
    timescale_ = DataVector{};
  }

  for (const auto& t_scale : timescale_) {
    if (t_scale <= 0.0) {
      ERROR("Initial timescale must be > 0");
    }
  }

  if (decrease_factor_ > 1.0 or decrease_factor <= 0.0) {
    ERROR("The specified decrease_factor "
          << decrease_factor_ << " must satisfy 0 < decrease_factor <= 1");
  }
  if (increase_factor_ < 1.0) {
    ERROR("The specified increase factor " << increase_factor_
                                           << " must be >= 1.0");
  }
  if (min_timescale_ <= 0.0) {
    ERROR("The specified minimum timescale " << min_timescale_
                                             << " must be > 0");
  }
  if (max_timescale_ <= min_timescale_) {
    ERROR("The maximum timescale "
          << max_timescale_
          << " must be > than the specified minimum timescale "
          << min_timescale_);
  }
  if (increase_timescale_threshold_ <= 0.0) {
    ERROR("The specified increase-timescale threshold "
          << increase_timescale_threshold_ << " must be > 0");
  }
  if (decrease_timescale_threshold_ <= increase_timescale_threshold_) {
    ERROR("The decrease-timescale threshold "
          << decrease_timescale_threshold_
          << " must be > than the specified increase-timescale threshold "
          << increase_timescale_threshold_);
  }
}

const DataVector& TimescaleTuner::current_timescale() const {
  check_if_timescales_have_been_set();
  return timescale_;
}

void TimescaleTuner::resize_timescales(
    const size_t num_timescales, const std::optional<double>& fill_value) {
  ASSERT(num_timescales > 0,
         "Damping timescales must have a non-zero number of components.");
  timescale_ = DataVector{num_timescales};

  set_timescale_if_in_allowable_range(fill_value.value_or(initial_timescale_));
}

void TimescaleTuner::set_timescale_if_in_allowable_range(
    const double suggested_timescale) {
  for (auto& t_scale : timescale_) {
    t_scale = std::clamp(suggested_timescale, min_timescale_, max_timescale_);
  }

  timescales_have_been_set_ = true;
}

void TimescaleTuner::update_timescale(
    const std::array<DataVector, 2>& q_and_dtq) {
  check_if_timescales_have_been_set();
  ASSERT(q_and_dtq[0].size() == timescale_.size() and
             q_and_dtq[1].size() == timescale_.size(),
         "One or both of the number of components in q_and_dtq("
             << q_and_dtq[0].size() << "," << q_and_dtq[1].size()
             << ") is inconsistent with the number of timescales("
             << timescale_.size() << ")");

  const DataVector& q = gsl::at(q_and_dtq, 0);
  const DataVector& dtq = gsl::at(q_and_dtq, 1);

  for (size_t i = 0; i < q.size(); i++) {
    // check whether we need to decrease the timescale:
    if ((fabs(q[i]) > decrease_timescale_threshold_ or
         fabs(dtq[i] * timescale_[i]) > decrease_timescale_threshold_) and
        (dtq[i] * q[i] > 0.0 or
         fabs(dtq[i]) * timescale_[i] < 0.5 * fabs(q[i]))) {
      // the first check is if Q `or` dtQ are above the maximum tolerance.
      // the second condition of the `and` is
      // that Q and dtQ are the same sign (the error is growing)
      // `or` that Q is not expected to drop to half of its current value in
      // one timescale (not decreasing fast enough)
      timescale_[i] *= decrease_factor_;
    }
    // check whether we need to increase the timescale:
    else if (fabs(q[i]) < increase_timescale_threshold_ and
             fabs(dtq[i] * timescale_[i]) <
                 (increase_timescale_threshold_ - fabs(q[i]))) {
      // if Q `and` dtQ are below the minimum required threshold
      timescale_[i] *= increase_factor_;
    }

    // make sure the timescale has not increased(decreased) above(below) the
    // maximum(minimum) value.
    timescale_[i] = std::clamp(timescale_[i], min_timescale_, max_timescale_);
  }
}

void TimescaleTuner::check_if_timescales_have_been_set() const {
  ASSERT(timescales_have_been_set_,
         "Damping timescales in the TimescaleTuner have not been set yet.");
}

void TimescaleTuner::pup(PUP::er& p) {
  p | timescale_;
  p | timescales_have_been_set_;
  p | initial_timescale_;
  p | max_timescale_;
  p | min_timescale_;
  p | decrease_timescale_threshold_;
  p | increase_timescale_threshold_;
  p | increase_factor_;
  p | decrease_factor_;
}

bool operator==(const TimescaleTuner& lhs, const TimescaleTuner& rhs) {
  return (lhs.timescale_ == rhs.timescale_) and
         (lhs.max_timescale_ == rhs.max_timescale_) and
         (lhs.min_timescale_ == rhs.min_timescale_) and
         (lhs.timescales_have_been_set_ == rhs.timescales_have_been_set_) and
         (lhs.initial_timescale_ == rhs.initial_timescale_) and
         (lhs.decrease_timescale_threshold_ ==
          rhs.decrease_timescale_threshold_) and
         (lhs.increase_timescale_threshold_ ==
          rhs.increase_timescale_threshold_) and
         (lhs.increase_factor_ == rhs.increase_factor_) and
         (lhs.decrease_factor_ == rhs.decrease_factor_);
}

bool operator!=(const TimescaleTuner& lhs, const TimescaleTuner& rhs) {
  return not(lhs == rhs);
}
