// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Domain/FunctionsOfTime/PiecewisePolynomial.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <ostream>
#include <pup.h>
#include <pup_stl.h>
#include <utility>  // IWYU pragma: keep
#include <vector>

#include "DataStructures/DataVector.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Literals.hpp"
#include "Utilities/MakeArray.hpp"
#include "Utilities/Serialization/PupStlCpp11.hpp"

namespace domain::FunctionsOfTime {
template <size_t MaxDeriv>
PiecewisePolynomial<MaxDeriv>::PiecewisePolynomial(
    const double t, value_type initial_func_and_derivs,
    const double expiration_time)
    : deriv_info_at_update_times_{{t, std::move(initial_func_and_derivs)}},
      expiration_time_(expiration_time) {
  deriv_info_size_.store(1, std::memory_order_release);
  expiration_time_.store(expiration_time, std::memory_order_release);
}

template <size_t MaxDeriv>
PiecewisePolynomial<MaxDeriv>::PiecewisePolynomial(
    PiecewisePolynomial<MaxDeriv>&& rhs) {
  *this = std::move(rhs);
}

template <size_t MaxDeriv>
PiecewisePolynomial<MaxDeriv>& PiecewisePolynomial<MaxDeriv>::operator=(
    PiecewisePolynomial<MaxDeriv>&& rhs) {
  if (this == &rhs) {
    return *this;
  }
  deriv_info_at_update_times_ = std::move(rhs.deriv_info_at_update_times_);
  expiration_time_.store(
      rhs.expiration_time_.exchange(0, std::memory_order_acq_rel),
      std::memory_order_release);
  deriv_info_size_.store(
      rhs.deriv_info_size_.exchange(0, std::memory_order_acq_rel),
      std::memory_order_release);
  return *this;
}

template <size_t MaxDeriv>
PiecewisePolynomial<MaxDeriv>::PiecewisePolynomial(
    const PiecewisePolynomial<MaxDeriv>& rhs) {
  *this = rhs;
}

template <size_t MaxDeriv>
// NOLINTNEXTLINE(bugprone-unhandled-self-assignment)
PiecewisePolynomial<MaxDeriv>& PiecewisePolynomial<MaxDeriv>::operator=(
    const PiecewisePolynomial<MaxDeriv>& rhs) {
  if (this == &rhs) {
    return *this;
  }
  deriv_info_at_update_times_ = rhs.deriv_info_at_update_times_;
  expiration_time_.store(rhs.expiration_time_.load(std::memory_order_acquire),
                         std::memory_order_release);
  deriv_info_size_.store(rhs.deriv_info_size_.load(std::memory_order_acquire),
                         std::memory_order_release);
  return *this;
}

template <size_t MaxDeriv>
std::unique_ptr<FunctionOfTime> PiecewisePolynomial<MaxDeriv>::get_clone()
    const {
  return std::make_unique<PiecewisePolynomial>(*this);
}

template <size_t MaxDeriv>
template <size_t MaxDerivReturned>
std::array<DataVector, MaxDerivReturned + 1>
PiecewisePolynomial<MaxDeriv>::func_and_derivs(
    const double t, const bool check_expiration_time) const {
  if (check_expiration_time and
      t > expiration_time_.load(std::memory_order_acquire)) {
    ERROR("Attempt to evaluate PiecewisePolynomial at a time "
          << t << " that is after the expiration time " << expiration_time_
          << ". The difference between times is " << t - expiration_time_
          << ".");
  }
  const auto& deriv_info_at_t = stored_info_from_upper_bound(
      t, deriv_info_at_update_times_,
      deriv_info_size_.load(std::memory_order_acquire));
  const double dt = t - deriv_info_at_t.time;
  const value_type& coefs = deriv_info_at_t.stored_quantities;

  // initialize result for the number of derivs requested
  std::array<DataVector, MaxDerivReturned + 1> result =
      make_array<MaxDerivReturned + 1>(DataVector(coefs.back().size(), 0.0));

  // evaluate the polynomial using ddpoly (Numerical Recipes sec 5.1)
  result[0] = coefs[MaxDeriv];
  for (size_t j = MaxDeriv; j-- > 0;) {
    const size_t min_deriv = std::min(MaxDerivReturned, MaxDeriv - j);
    for (size_t k = min_deriv; k > 0; k--) {
      gsl::at(result, k) = gsl::at(result, k) * dt + gsl::at(result, k - 1);
    }
    result[0] = result[0] * dt + gsl::at(coefs, j);
  }
  // after the first derivative, factorial constants come in
  double fact = 1.0;
  for (size_t j = 2; j < MaxDerivReturned + 1; j++) {
    fact *= j;
    gsl::at(result, j) *= fact;
  }

  return result;
}

template <size_t MaxDeriv>
void PiecewisePolynomial<MaxDeriv>::update(
    // Clang-tidy says to use 'const DataVector& updated_max_deriv'.
    // However, updated_max_deriv is std::moved out of inside this function.
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    const double time_of_update, DataVector updated_max_deriv,
    const double next_expiration_time) {
  const std::lock_guard<std::mutex> update_lock{update_mutex_};
  double current_expiration_time =
      expiration_time_.load(std::memory_order_acquire);
  // We can use `.back()` here because we only allow one thread to call update
  // at once
  if (time_of_update <= deriv_info_at_update_times_.back().time) {
    ERROR("t must be increasing from call to call. "
          << "Attempted to update at time " << time_of_update
          << ", which precedes the previous update time of "
          << deriv_info_at_update_times_.back().time << ".");
  }
  if (next_expiration_time < current_expiration_time) {
    ERROR("expiration_time must be nondecreasing from call to call. "
          << "Attempted to change expiration time to " << next_expiration_time
          << ", which precedes the previous expiration time of "
          << current_expiration_time << ".");
  }
  if (time_of_update < current_expiration_time) {
    ERROR("Attempt to update PiecewisePolynomial at a time "
          << time_of_update
          << " that is earlier than the previous expiration time of "
          << expiration_time_
          << ". This is bad because some asynchronous process may have already "
             "used PiecewisePolynomial at a time later than the current time "
          << time_of_update << ".");
  }
  if (time_of_update > next_expiration_time) {
    ERROR(
        "Attempt to set the expiration time of PiecewisePolynomial "
        "to a value "
        << next_expiration_time << " that is earlier than the current time "
        << time_of_update << ".");
  }

  // Normally, func_and_derivs(t) throws an error if t>expiration_time_.
  // But here, we want to allow time_of_update to be greater than the *previous*
  // expiration time, so we just ignore the expiration time check. We get the
  // current values before updating the `MaxDeriv'th deriv
  value_type func = func_and_derivs(time_of_update, false);

  if (updated_max_deriv.size() != func.back().size()) {
    ERROR("the number of components trying to be updated ("
          << updated_max_deriv.size()
          << ") does "
             "not match the number of components ("
          << func.back().size() << ") in the PiecewisePolynomial.");
  }

  func[MaxDeriv] = std::move(updated_max_deriv);
  // While it is technically possible for a different thread to call `update`
  // during this call, there should never be two different threads trying to
  // update this object at the same time in our algorithm. An update should come
  // from only one place and nowhere else so this emplacement should be ok
  //
  // The order of these next three changes are important to maintain
  // thread-safety and avoid a race condition.
  //
  //  1. Add the updated deriv info to the back of the list, keeping the valid
  //     size of the list to use and the expiration time the same.
  //  2. Update the valid size to match the new size of the list, keeping the
  //     expiration time the same.
  //  3. Update the expiration time
  //
  // The expiration time is exposed via `time_bounds` and used to determine
  // whether it's safe to call the `func`s at a specific time. If we update the
  // expiration time first, then this class will be in an invalid state because
  // we have not yet updated the stored values; we have the possibility of using
  // old values at a time past the old expiration time on one thread, but new
  // values at a time past the old expiration time on a different thread. Thus
  // we must update the expiration time last.
  //
  // We also want to ensure that we are accessing only valid deriv infos. Thus
  // we need to update the valid size *after* we emplace the updated deriv info
  // to ensure everything is consistent. This gives us the ordering above.
  deriv_info_at_update_times_.emplace_back(time_of_update, std::move(func));
  deriv_info_size_.fetch_add(1, std::memory_order_acq_rel);
  if (not expiration_time_.compare_exchange_strong(current_expiration_time,
                                                   next_expiration_time,
                                                   std::memory_order_acq_rel)) {
    ERROR("PiecewisePolynomial could not exchange the current expiration time "
          << current_expiration_time << " for the new expiration time "
          << next_expiration_time);
  }
}

template <size_t MaxDeriv>
void PiecewisePolynomial<MaxDeriv>::pup(PUP::er& p) {
  FunctionOfTime::pup(p);
  size_t version = 2;
  p | version;
  // Remember to increment the version number when making changes to this
  // function. Retain support for unpacking data written by previous versions
  // whenever possible. See `Domain` docs for details.

  // Here is where we must retain support for older versions
  if (p.isUnpacking()) {
    // For versions 0 and 1, we stored the data first, then expiration time,
    // then possibly the size
    if (version <= 1) {
      if (version == 0) {
        // Version 0 had a std::vector rather than a std::list
        std::vector<FunctionOfTimeHelpers::StoredInfo<MaxDeriv + 1>>
            pupped_info{};
        p | pupped_info;
        for (auto& pupped_deriv_info : pupped_info) {
          deriv_info_at_update_times_.emplace_back(
              std::move(pupped_deriv_info));
        }
      } else {
        // Version 1 had a std::deque rather than a std::list
        std::deque<FunctionOfTimeHelpers::StoredInfo<MaxDeriv + 1>>
            pupped_info{};
        p | pupped_info;
        for (auto& pupped_deriv_info : pupped_info) {
          deriv_info_at_update_times_.emplace_back(
              std::move(pupped_deriv_info));
        }
      }

      // Same for v0 and v1
      p | expiration_time_;

      if (version == 0) {
        deriv_info_size_.store(deriv_info_at_update_times_.size(),
                               std::memory_order_release);
      } else {
        p | deriv_info_size_;
      }
    } else if (version >= 2) {
      // However, for v2+, we store expiration time, size, then data for
      // thread-safety reasons
      p | expiration_time_;
      size_t size = 0;
      p | size;
      deriv_info_size_.store(size, std::memory_order_release);
      deriv_info_at_update_times_.clear();
      deriv_info_at_update_times_.resize(
          deriv_info_size_.load(std::memory_order_acquire));
      // Using range-based loop here is ok because we won't be updating while
      // packing/unpacking
      for (auto& deriv_info : deriv_info_at_update_times_) {
        p | deriv_info;
      }
    }
  } else {
    p | expiration_time_;
    // This is guaranteed to be thread-safe for both packing and sizing
    size_t size = deriv_info_size_.load(std::memory_order_acquire);
    p | size;
    auto it = deriv_info_at_update_times_.begin();
    for (size_t i = 0; i < size; i++, it++) {
      p | *it;
    }
  }
}

template <size_t MaxDeriv>
bool operator==(const PiecewisePolynomial<MaxDeriv>& lhs,
                const PiecewisePolynomial<MaxDeriv>& rhs) {
  return lhs.deriv_info_at_update_times_ == rhs.deriv_info_at_update_times_ and
         lhs.expiration_time_ == rhs.expiration_time_ and
         lhs.deriv_info_size_ == rhs.deriv_info_size_;
}

template <size_t MaxDeriv>
bool operator!=(const PiecewisePolynomial<MaxDeriv>& lhs,
                const PiecewisePolynomial<MaxDeriv>& rhs) {
  return not(lhs == rhs);
}

template <size_t MaxDeriv>
std::ostream& operator<<(
    std::ostream& os,
    const PiecewisePolynomial<MaxDeriv>& piecewise_polynomial) {
  const size_t writable_size =
      piecewise_polynomial.deriv_info_size_.load(std::memory_order_acquire);
  auto it = piecewise_polynomial.deriv_info_at_update_times_.begin();
  // Can't use .end() because of thread-safety and don't want to do expensive
  // std::next() call, so we just loop over allowed size while incrementing an
  // iterator
  for (size_t i = 0; i < writable_size; i++, it++) {
    os << *it;
    // writable_size guaranteed to be >= 1. No need to worry about negative
    // size_t
    if (i != writable_size - 1) {
      os << "\n";
    }
  }
  return os;
}

// do explicit instantiation of MaxDeriv = {0,1,2,3,4}
// along with all combinations of MaxDerivReturned = {0,...,MaxDeriv}
#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)
#define DIMRETURNED(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATE(_, data)                                                  \
  template class PiecewisePolynomial<DIM(data)>;                              \
  template bool operator==<DIM(data)>(const PiecewisePolynomial<DIM(data)>&,  \
                                      const PiecewisePolynomial<DIM(data)>&); \
  template bool operator!=<DIM(data)>(const PiecewisePolynomial<DIM(data)>&,  \
                                      const PiecewisePolynomial<DIM(data)>&); \
  template std::ostream& operator<<(                                          \
      std::ostream& os,                                                       \
      const PiecewisePolynomial<DIM(data)>& piecewise_polynomial);

GENERATE_INSTANTIATIONS(INSTANTIATE, (0, 1, 2, 3, 4))

#undef INSTANTIATE

#define INSTANTIATE(_, data)                                          \
  template std::array<DataVector, DIMRETURNED(data) + 1>              \
  PiecewisePolynomial<DIM(data)>::func_and_derivs<DIMRETURNED(data)>( \
      const double, const bool) const;

GENERATE_INSTANTIATIONS(INSTANTIATE, (0, 1, 2, 3, 4), (0, 1, 2))

#undef DIM
#undef DIMRETURNED
#undef INSTANTIATE
}  // namespace domain::FunctionsOfTime
