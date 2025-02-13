// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Domain/CoordinateMaps/TimeDependent/Translation.hpp"

#include <cmath>
#include <ostream>
#include <pup.h>
#include <pup_stl.h>
#include <unordered_set>
#include <utility>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/DeterminantAndInverse.hpp"
#include "DataStructures/Tensor/Identity.hpp"
#include "Domain/FunctionsOfTime/FunctionOfTime.hpp"
#include "NumericalAlgorithms/RootFinding/TOMS748.hpp"
#include "PointwiseFunctions/MathFunctions/MathFunction.hpp"
#include "Utilities/ContainerHelpers.hpp"
#include "Utilities/DereferenceWrapper.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeArray.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/StdArrayHelpers.hpp"
#include "Utilities/StdHelpers.hpp"

namespace domain::CoordinateMaps::TimeDependent {

template <size_t Dim>
Translation<Dim>::Translation(std::string function_of_time_name)
    : f_of_t_name_(std::move(function_of_time_name)),
      f_of_t_names_({f_of_t_name_}),
      f_of_r_(nullptr),
      center_(make_array<Dim, double>(0.0)) {}

template <size_t Dim>
Translation<Dim>::Translation(
    std::string function_of_time_name,
    std::unique_ptr<MathFunction<1, Frame::Inertial>> radial_function,
    std::array<double, Dim>& center)
    : f_of_t_name_(std::move(function_of_time_name)),
      f_of_t_names_({f_of_t_name_}),
      f_of_r_(std::move(radial_function)),
      center_(center) {}

template <size_t Dim>
Translation<Dim>::Translation(const Translation<Dim>& Translation_Map)
    : f_of_t_name_(Translation_Map.f_of_t_name_),
      f_of_t_names_(Translation_Map.f_of_t_names_),
      center_(Translation_Map.center_) {
  if (Translation_Map.f_of_r_ == nullptr) {
    f_of_r_ = nullptr;
  } else {
    f_of_r_ = Translation_Map.f_of_r_->get_clone();
  }
}

template <size_t Dim>
template <typename T>
std::array<tt::remove_cvref_wrap_t<T>, Dim> Translation<Dim>::operator()(
    const std::array<T, Dim>& source_coords, const double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time) const {
  return coord_helper(source_coords, time, functions_of_time, 0);
}

template <size_t Dim>
std::optional<std::array<double, Dim>> Translation<Dim>::inverse(
    const std::array<double, Dim>& target_coords, const double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time) const {
  std::array<double, Dim> result{};
  for (size_t i = 0; i < Dim; i++) {
    gsl::at(result, i) = gsl::at(target_coords, i);
  }
  const DataVector function_of_time =
      functions_of_time.at(f_of_t_name_)->func(time)[0];
  ASSERT(function_of_time.size() == Dim,
         "The dimension of the function of time ("
             << function_of_time.size()
             << ") does not match the dimension of the translation map (" << Dim
             << ").");
  double radial_function_value = 1.0;
  if (f_of_r_ != nullptr) {
    const double root = root_finder(target_coords - center_, function_of_time);
    radial_function_value = (*f_of_r_)(root);
  }
  for (size_t i = 0; i < Dim; i++) {
    gsl::at(result, i) -= function_of_time[i] * radial_function_value;
  }
  return result;
}

template <size_t Dim>
template <typename T>
std::array<tt::remove_cvref_wrap_t<T>, Dim> Translation<Dim>::frame_velocity(
    const std::array<T, Dim>& source_coords, const double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time) const {
  return coord_helper(source_coords, time, functions_of_time, 1);
}

template <size_t Dim>
template <typename T>
tnsr::Ij<tt::remove_cvref_wrap_t<T>, Dim, Frame::NoFrame>
Translation<Dim>::jacobian(
    const std::array<T, Dim>& source_coords, const double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time) const {
  if (f_of_r_ == nullptr) {
    return identity<Dim>(dereference_wrapper(source_coords[0]));
  } else {
    std::array<tt::remove_cvref_wrap_t<T>, Dim> distance_to_center{};
    for (size_t i = 0; i < Dim; i++) {
      gsl::at(distance_to_center, i) =
          gsl::at(source_coords, i) - gsl::at(center_, i);
    }
    auto radius = magnitude(distance_to_center);

    const DataVector function_of_time =
        functions_of_time.at(f_of_t_name_)->func(time)[0];

    auto result = make_with_value<
        tnsr::Ij<tt::remove_cvref_wrap_t<T>, Dim, Frame::NoFrame>>(
        dereference_wrapper(source_coords[0]), 0.0);
    for (size_t i = 0; i < Dim; i++) {
      for (size_t j = 0; j < Dim; j++) {
        for (size_t k = 0; k < get_size(radius); k++) {
          if (get_element(radius, k) > 1.e-13) {
            result.get(i, j) = (*f_of_r_).first_deriv(get_element(radius, k)) *
                               gsl::at(function_of_time, i) *
                               gsl::at(distance_to_center, j) /
                               get_element(radius, k);
          } else {
            result.get(i, j) = (*f_of_r_).second_deriv(get_element(radius, k)) *
                               gsl::at(function_of_time, i) *
                               gsl::at(distance_to_center, j);
          }
        }
      }
      result.get(i, i) += 1.0;
    }
    return result;
  }
}
template <size_t Dim>
template <typename T>
tnsr::Ij<tt::remove_cvref_wrap_t<T>, Dim, Frame::NoFrame>
Translation<Dim>::inv_jacobian(
    const std::array<T, Dim>& source_coords, const double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time) const {
  if (f_of_r_ == nullptr) {
    return identity<Dim>(dereference_wrapper(source_coords[0]));
  } else {
    return determinant_and_inverse(
               jacobian(source_coords, time, functions_of_time))
        .second;
  }
}

template <size_t Dim>
template <typename T>
std::array<tt::remove_cvref_wrap_t<T>, Dim> Translation<Dim>::coord_helper(
    const std::array<T, Dim>& source_coords, double time,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time,
    const size_t function_or_deriv_index) const {
  const auto func_or_deriv_of_time =
      gsl::at(functions_of_time.at(f_of_t_name_)->func_and_deriv(time),
              function_or_deriv_index);
  ASSERT(func_or_deriv_of_time.size() == Dim,
         "The dimension of the function of time ("
             << func_or_deriv_of_time.size()
             << ") does not match the dimension of the translation map (" << Dim
             << ").");
  std::array<tt::remove_cvref_wrap_t<T>, Dim> result{};
  // sizing the result and getting the radial function value
  for (size_t i = 0; i < Dim; i++) {
    gsl::at(result, i) = gsl::at(source_coords, i);
  }
  auto radial_function_value = make_with_value<tt::remove_cvref_wrap_t<T>>(
      dereference_wrapper(source_coords[0]), 1.0);
  if (f_of_r_ != nullptr) {
    radial_function_value = (*f_of_r_)(magnitude(result - center_));
  }
  if (function_or_deriv_index == 1) {
    for (size_t i = 0; i < Dim; i++) {
      gsl::at(result, i) = 0.0;
    }
  }
  for (size_t i = 0; i < Dim; i++) {
    gsl::at(result, i) +=
        gsl::at(func_or_deriv_of_time, i) * radial_function_value;
  }
  return result;
}

template <size_t Dim>
double Translation<Dim>::root_finder(
    const std::array<double, Dim>& distance_to_center,
    const DataVector& function_of_time) const {
  const double target_radius = magnitude(distance_to_center);
  const double center_offset = norm(function_of_time);
  // These bounds come from the SpEC implementation
  double lower_bound_on_r = (target_radius - center_offset) * (1.0 - 1.e-9);
  double upper_bound_on_r = (target_radius + center_offset) * (1.0 + 1.e-9);
  if (lower_bound_on_r < 0.) {
    lower_bound_on_r = 0.;
  }
  const double absolute_tol = std::numeric_limits<double>::epsilon() *
                              std::max(target_radius, center_offset);
  const double relative_tol = 2.0 * std::numeric_limits<double>::epsilon();
  const auto wrapper = [this, &distance_to_center,
                        &function_of_time](const double r_squared) {
    // This form makes it clear that wrapper(0.0) <= 0.0, even with
    // roundoff, so it is guaranteed that the lower end of the bracket
    // is valid even when we have to correct for negative radii.
    const double f_of_r = (*f_of_r_)(sqrt(r_squared));
    double expression_to_zero = r_squared;
    for (size_t i = 0; i < Dim; ++i) {
      expression_to_zero -=
          square(gsl::at(distance_to_center, i) - f_of_r * function_of_time[i]);
    }
    return expression_to_zero;
  };
  return sqrt(RootFinder::toms748(wrapper, square(lower_bound_on_r),
                                  square(upper_bound_on_r), absolute_tol,
                                  relative_tol));
}

template <size_t Dim>
void Translation<Dim>::pup(PUP::er& p) {
  size_t version = 1;
  p | version;
  // Remember to increment the version number when making changes to this
  // function. Retain support for unpacking data written by previous versions
  // whenever possible. See `Domain` docs for details.
  if (version >= 0) {
    p | f_of_t_name_;
  }
  if (version >= 1) {
    p | f_of_r_;
    p | center_;
  } else if (p.isUnpacking()) {
    f_of_r_ = nullptr;
    center_ = make_array<Dim, double>(0.0);
  }

  // No need to pup this because it is uniquely determined by f_of_t_name_
  if (p.isUnpacking()) {
    f_of_t_names_.clear();
    f_of_t_names_.insert(f_of_t_name_);
  }
}

template <size_t Dim>
bool operator==(const Translation<Dim>& lhs, const Translation<Dim>& rhs) {
  return lhs.f_of_t_name_ == rhs.f_of_t_name_ and
         (lhs.f_of_r_ == nullptr) == (rhs.f_of_r_ == nullptr) and
         ((lhs.f_of_r_ == nullptr and rhs.f_of_r_ == nullptr) or
          *lhs.f_of_r_ == *rhs.f_of_r_) and
         lhs.center_ == rhs.center_;
}

// Explicit instantiations
#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)

#define INSTANTIATE(_, data)                              \
  template class Translation<DIM(data)>;                  \
  template bool operator==(const Translation<DIM(data)>&, \
                           const Translation<DIM(data)>&);

GENERATE_INSTANTIATIONS(INSTANTIATE, (1, 2, 3))

#undef INSTANTIATE

#define DTYPE(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATE(_, data)                                                \
  template std::array<tt::remove_cvref_wrap_t<DTYPE(data)>, DIM(data)>      \
  Translation<DIM(data)>::operator()(                                       \
      const std::array<DTYPE(data), DIM(data)>& source_coords, double time, \
      const std::unordered_map<                                             \
          std::string,                                                      \
          std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&        \
          functions_of_time) const;                                         \
  template std::array<tt::remove_cvref_wrap_t<DTYPE(data)>, DIM(data)>      \
  Translation<DIM(data)>::frame_velocity(                                   \
      const std::array<DTYPE(data), DIM(data)>& source_coords, double time, \
      const std::unordered_map<                                             \
          std::string,                                                      \
          std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&        \
          functions_of_time) const;                                         \
  template tnsr::Ij<tt::remove_cvref_wrap_t<DTYPE(data)>, DIM(data),        \
                    Frame::NoFrame>                                         \
  Translation<DIM(data)>::jacobian(                                         \
      const std::array<DTYPE(data), DIM(data)>& source_coords, double time, \
      const std::unordered_map<                                             \
          std::string,                                                      \
          std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&        \
          functions_of_time) const;                                         \
  template tnsr::Ij<tt::remove_cvref_wrap_t<DTYPE(data)>, DIM(data),        \
                    Frame::NoFrame>                                         \
  Translation<DIM(data)>::inv_jacobian(                                     \
      const std::array<DTYPE(data), DIM(data)>& source_coords, double time, \
      const std::unordered_map<                                             \
          std::string,                                                      \
          std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&        \
          functions_of_time) const;

GENERATE_INSTANTIATIONS(INSTANTIATE, (1, 2, 3),
                        (double, DataVector,
                         std::reference_wrapper<const double>,
                         std::reference_wrapper<const DataVector>))
#undef DIM
#undef DTYPE
#undef INSTANTIATE
}  // namespace domain::CoordinateMaps::TimeDependent
