// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "PointwiseFunctions/MathFunctions/Gaussian.hpp"

#include <cmath>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"

namespace MathFunctions {

template <typename Fr>
Gaussian<1, Fr>::Gaussian(const double amplitude, const double width,
                          const double center)
    : amplitude_(amplitude), inverse_width_(1.0 / width), center_(center) {}

template <typename Fr>
Gaussian<1, Fr>::Gaussian(double amplitude, double width,
                          const std::array<double, 1>& center)
    : Gaussian(amplitude, width, center[0]) {}

template <typename Fr>
std::unique_ptr<MathFunction<1, Fr>> Gaussian<1, Fr>::get_clone() const {
  return std::make_unique<Gaussian<1, Fr>>(*this);
}

template <typename Fr>
double Gaussian<1, Fr>::operator()(const double& x) const {
  return apply_call_operator(x);
}

template <typename Fr>
DataVector Gaussian<1, Fr>::operator()(const DataVector& x) const {
  return apply_call_operator(x);
}

template <typename Fr>
double Gaussian<1, Fr>::first_deriv(const double& x) const {
  return apply_first_deriv(x);
}

template <typename Fr>
DataVector Gaussian<1, Fr>::first_deriv(const DataVector& x) const {
  return apply_first_deriv(x);
}

template <typename Fr>
double Gaussian<1, Fr>::second_deriv(const double& x) const {
  return apply_second_deriv(x);
}

template <typename Fr>
DataVector Gaussian<1, Fr>::second_deriv(const DataVector& x) const {
  return apply_second_deriv(x);
}

template <typename Fr>
double Gaussian<1, Fr>::third_deriv(const double& x) const {
  return apply_third_deriv(x);
}

template <typename Fr>
DataVector Gaussian<1, Fr>::third_deriv(const DataVector& x) const {
  return apply_third_deriv(x);
}

template <typename Fr>
template <typename T>
T Gaussian<1, Fr>::apply_call_operator(const T& x) const {
  return amplitude_ * exp(-square((x - center_) * inverse_width_));
}

template <typename Fr>
template <typename T>
T Gaussian<1, Fr>::apply_first_deriv(const T& x) const {
  return (-2.0 * amplitude_ * square(inverse_width_)) * (x - center_) *
         exp(-square((x - center_) * inverse_width_));
}

template <typename Fr>
template <typename T>
T Gaussian<1, Fr>::apply_second_deriv(const T& x) const {
  return (-2.0 * amplitude_ * square(inverse_width_)) *
         (1.0 - 2.0 * square(x - center_) * square(inverse_width_)) *
         exp(-square((x - center_) * inverse_width_));
}

template <typename Fr>
template <typename T>
T Gaussian<1, Fr>::apply_third_deriv(const T& x) const {
  return 4.0 * amplitude_ * pow<4>(inverse_width_) * (x - center_) *
         (3.0 - 2.0 * square((x - center_) * inverse_width_)) *
         exp(-square((x - center_) * inverse_width_));
}

template <typename Fr>
void Gaussian<1, Fr>::pup(PUP::er& p) {
  MathFunction<1, Fr>::pup(p);
  p | amplitude_;
  p | inverse_width_;
  p | center_;
}

template <typename Fr>
bool Gaussian<1, Fr>::operator==(const MathFunction<1, Fr>& other) const {
  const auto* derived_other = dynamic_cast<const Gaussian<1, Fr>*>(&other);
  if (derived_other != nullptr) {
    return (this->amplitude_ == derived_other->amplitude_) and
           (this->inverse_width_ == derived_other->inverse_width_) and
           (this->center_ == derived_other->center_);
  }
  // LCOV_EXCL_START
  return false;
  // LCOV_EXCL_STOP
}

template <typename Fr>
bool Gaussian<1, Fr>::operator!=(const MathFunction<1, Fr>& other) const {
  return not(*this == other);
}

template <size_t VolumeDim, typename Fr>
Gaussian<VolumeDim, Fr>::Gaussian(const double amplitude, const double width,
                                  const std::array<double, VolumeDim>& center)
    : amplitude_(amplitude), inverse_width_(1.0 / width), center_(center) {}

template <size_t VolumeDim, typename Fr>
std::unique_ptr<MathFunction<VolumeDim, Fr>>
Gaussian<VolumeDim, Fr>::get_clone() const {
  return std::make_unique<Gaussian<VolumeDim, Fr>>(*this);
}

template <size_t VolumeDim, typename Fr>
template <typename T>
tnsr::I<T, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::centered_coordinates(
    const tnsr::I<T, VolumeDim, Fr>& x) const {
  tnsr::I<T, VolumeDim, Fr> centered_coords = x;
  for (size_t i = 0; i < VolumeDim; ++i) {
    centered_coords.get(i) -= gsl::at(center_, i);
  }
  return centered_coords;
}

template <size_t VolumeDim, typename Fr>
template <typename T>
Scalar<T> Gaussian<VolumeDim, Fr>::apply_call_operator(
    const tnsr::I<T, VolumeDim, Fr>& centered_coords) const {
  Scalar<T> result = dot_product(centered_coords, centered_coords);
  get(result) = amplitude_ * exp(-get(result) * square(inverse_width_));
  return result;
}

template <size_t VolumeDim, typename Fr>
template <typename T>
tnsr::i<T, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::apply_first_deriv(
    const tnsr::I<T, VolumeDim, Fr>& centered_coords,
    const Scalar<T>& gaussian) const {
  auto result = make_with_value<tnsr::i<T, VolumeDim, Fr>>(get(gaussian), 0.0);
  for (size_t i = 0; i < VolumeDim; ++i) {
    result.get(i) =
        -2.0 * square(inverse_width_) * get(gaussian) * centered_coords.get(i);
  }
  return result;
}

template <size_t VolumeDim, typename Fr>
template <typename T>
tnsr::ii<T, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::apply_second_deriv(
    const tnsr::I<T, VolumeDim, Fr>& centered_coords, const Scalar<T>& gaussian,
    const tnsr::i<T, VolumeDim, Fr>& d_gaussian) const {
  auto result = make_with_value<tnsr::ii<T, VolumeDim, Fr>>(get(gaussian), 0.0);
  for (size_t i = 0; i < VolumeDim; ++i) {
    for (size_t j = i; j < VolumeDim; ++j) {
      result.get(i, j) = centered_coords.get(i) * d_gaussian.get(j);
      if (i == j) {
        result.get(i, i) += get(gaussian);
      }
      result.get(i, j) *= -2.0 * square(inverse_width_);
    }
  }
  return result;
}

template <size_t VolumeDim, typename Fr>
template <typename T>
tnsr::iii<T, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::apply_third_deriv(
    const tnsr::I<T, VolumeDim, Fr>& centered_coords, const Scalar<T>& gaussian,
    const tnsr::i<T, VolumeDim, Fr>& d_gaussian,
    const tnsr::ii<T, VolumeDim, Fr>& d2_gaussian) const {
  auto result =
      make_with_value<tnsr::iii<T, VolumeDim, Fr>>(get(gaussian), 0.0);
  for (size_t i = 0; i < VolumeDim; ++i) {
    for (size_t j = i; j < VolumeDim; ++j) {
      for (size_t k = j; k < VolumeDim; ++k) {
        result.get(i, j, k) = centered_coords.get(j) * d2_gaussian.get(i, k);
        if (j == k) {
          result.get(i, j, k) += d_gaussian.get(i);
        }
        if (i == j) {
          result.get(i, j, k) += d_gaussian.get(k);
        }
        result.get(i, j, k) *= -2.0 * square(inverse_width_);
      }
    }
  }
  return result;
}

template <size_t VolumeDim, typename Fr>
Scalar<double> Gaussian<VolumeDim, Fr>::operator()(
    const tnsr::I<double, VolumeDim, Fr>& x) const {
  return apply_call_operator(centered_coordinates(x));
}
template <size_t VolumeDim, typename Fr>
Scalar<DataVector> Gaussian<VolumeDim, Fr>::operator()(
    const tnsr::I<DataVector, VolumeDim, Fr>& x) const {
  return apply_call_operator(centered_coordinates(x));
}

template <size_t VolumeDim, typename Fr>
tnsr::i<double, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::first_deriv(
    const tnsr::I<double, VolumeDim, Fr>& x) const {
  const tnsr::I<double, VolumeDim, Fr>& centered_coords =
      centered_coordinates(x);
  return apply_first_deriv(centered_coords,
                           apply_call_operator(centered_coords));
}
template <size_t VolumeDim, typename Fr>
tnsr::i<DataVector, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::first_deriv(
    const tnsr::I<DataVector, VolumeDim, Fr>& x) const {
  const tnsr::I<DataVector, VolumeDim, Fr>& centered_coords =
      centered_coordinates(x);
  return apply_first_deriv(centered_coords,
                           apply_call_operator(centered_coords));
}

template <size_t VolumeDim, typename Fr>
tnsr::ii<double, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::second_deriv(
    const tnsr::I<double, VolumeDim, Fr>& x) const {
  const tnsr::I<double, VolumeDim, Fr> centered_coords =
      centered_coordinates(x);
  const Scalar<double>& gauss = apply_call_operator(centered_coords);
  return apply_second_deriv(centered_coords, gauss,
                            apply_first_deriv(centered_coords, gauss));
}
template <size_t VolumeDim, typename Fr>
tnsr::ii<DataVector, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::second_deriv(
    const tnsr::I<DataVector, VolumeDim, Fr>& x) const {
  const tnsr::I<DataVector, VolumeDim, Fr> centered_coords =
      centered_coordinates(x);
  const Scalar<DataVector>& gauss = apply_call_operator(centered_coords);
  return apply_second_deriv(centered_coords, gauss,
                            apply_first_deriv(centered_coords, gauss));
}

template <size_t VolumeDim, typename Fr>
tnsr::iii<double, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::third_deriv(
    const tnsr::I<double, VolumeDim, Fr>& x) const {
  const tnsr::I<double, VolumeDim, Fr> centered_coords =
      centered_coordinates(x);
  const Scalar<double>& gauss = apply_call_operator(centered_coords);
  const tnsr::i<double, VolumeDim, Fr> d_gauss =
      apply_first_deriv(centered_coords, gauss);
  return apply_third_deriv(centered_coords, gauss, d_gauss,
                           apply_second_deriv(centered_coords, gauss, d_gauss));
}
template <size_t VolumeDim, typename Fr>
tnsr::iii<DataVector, VolumeDim, Fr> Gaussian<VolumeDim, Fr>::third_deriv(
    const tnsr::I<DataVector, VolumeDim, Fr>& x) const {
  const tnsr::I<DataVector, VolumeDim, Fr> centered_coords =
      centered_coordinates(x);
  const Scalar<DataVector>& gauss = apply_call_operator(centered_coords);
  const tnsr::i<DataVector, VolumeDim, Fr> d_gauss =
      apply_first_deriv(centered_coords, gauss);
  return apply_third_deriv(centered_coords, gauss, d_gauss,
                           apply_second_deriv(centered_coords, gauss, d_gauss));
}

template <size_t VolumeDim, typename Fr>
bool Gaussian<VolumeDim, Fr>::operator==(
    const MathFunction<VolumeDim, Fr>& other) const {
  const auto* derived_other =
      dynamic_cast<const Gaussian<VolumeDim, Fr>*>(&other);
  if (derived_other != nullptr) {
    return this->amplitude_ == derived_other->amplitude_ and
           this->inverse_width_ == derived_other->inverse_width_ and
           this->center_ == derived_other->center_;
  }
  // LCOV_EXCL_START
  return false;
  // LCOV_EXCL_STOP
}

template <size_t VolumeDim, typename Fr>
bool Gaussian<VolumeDim, Fr>::operator!=(
    const MathFunction<VolumeDim, Fr>& other) const {
  return not(*this == other);
}

template <size_t VolumeDim, typename Fr>
void Gaussian<VolumeDim, Fr>::pup(PUP::er& p) {
  MathFunction<VolumeDim, Fr>::pup(p);
  p | amplitude_;
  p | inverse_width_;
  p | center_;
}
}  // namespace MathFunctions

#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)
#define FRAME(data) BOOST_PP_TUPLE_ELEM(1, data)
#define INSTANTIATE(_, data) \
  template class MathFunctions::Gaussian<DIM(data), FRAME(data)>;

GENERATE_INSTANTIATIONS(INSTANTIATE, (1, 2, 3), (Frame::Grid, Frame::Inertial))

#undef DIM
#undef FRAME
#undef INSTANTIATE

#define FRAME(data) BOOST_PP_TUPLE_ELEM(0, data)
#define DTYPE(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATE(_, data)                                        \
  template DTYPE(data)                                              \
      MathFunctions::Gaussian<1, FRAME(data)>::apply_call_operator( \
          const DTYPE(data) & x) const;                             \
  template DTYPE(data)                                              \
      MathFunctions::Gaussian<1, FRAME(data)>::apply_first_deriv(   \
          const DTYPE(data) & x) const;                             \
  template DTYPE(data)                                              \
      MathFunctions::Gaussian<1, FRAME(data)>::apply_second_deriv(  \
          const DTYPE(data) & x) const;                             \
  template DTYPE(data)                                              \
      MathFunctions::Gaussian<1, FRAME(data)>::apply_third_deriv(   \
          const DTYPE(data) & x) const;

GENERATE_INSTANTIATIONS(INSTANTIATE, (Frame::Grid, Frame::Inertial),
                        (double, DataVector))
#undef DTYPE
#undef FRAME
#undef INSTANTIATE

#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)
#define FRAME(data) BOOST_PP_TUPLE_ELEM(1, data)
#define DTYPE(data) BOOST_PP_TUPLE_ELEM(2, data)

#define INSTANTIATE(_, data)                                                 \
  template tnsr::I<DTYPE(data), DIM(data), FRAME(data)>                      \
  MathFunctions::Gaussian<DIM(data), FRAME(data)>::centered_coordinates(     \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& x) const;          \
  template Scalar<DTYPE(data)>                                               \
  MathFunctions::Gaussian<DIM(data), FRAME(data)>::apply_call_operator(      \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& x) const;          \
  template tnsr::i<DTYPE(data), DIM(data), FRAME(data)>                      \
  MathFunctions::Gaussian<DIM(data), FRAME(data)>::apply_first_deriv(        \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& x,                 \
      const Scalar<DTYPE(data)>& gaussian) const;                            \
  template tnsr::ii<DTYPE(data), DIM(data), FRAME(data)>                     \
  MathFunctions::Gaussian<DIM(data), FRAME(data)>::apply_second_deriv(       \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& x,                 \
      const Scalar<DTYPE(data)>& gaussian,                                   \
      const tnsr::i<DTYPE(data), DIM(data), FRAME(data)>& d_gaussian) const; \
  template tnsr::iii<DTYPE(data), DIM(data), FRAME(data)>                    \
  MathFunctions::Gaussian<DIM(data), FRAME(data)>::apply_third_deriv(        \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& x,                 \
      const Scalar<DTYPE(data)>& gaussian,                                   \
      const tnsr::i<DTYPE(data), DIM(data), FRAME(data)>& d_gaussian,        \
      const tnsr::ii<DTYPE(data), DIM(data), FRAME(data)>& d2_gaussian) const;

GENERATE_INSTANTIATIONS(INSTANTIATE, (2, 3), (Frame::Grid, Frame::Inertial),
                        (double, DataVector))
#undef DTYPE
#undef FRAME
#undef INSTANTIATE
