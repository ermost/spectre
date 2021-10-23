// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>

#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace gsl {
template <typename>
struct not_null;
}  // namespace gsl
/// \endcond

namespace gr {

/// @{
/*!
 * \ingroup GeneralRelativityGroup
 * \brief Computes the magnetic part of the Weyl tensor.
 *
 * \details Computes the magnetic part of the Weyl tensor \f$B_{ij}\f$
 * as:
 *
 * \f{align}{
 * B_{ij} =
 * \left(1/\sqrt{\det\gamma}\right)D_{k}K_{l(i}\gamma_{j)m}\epsilon^{mlk} \f}
 *
 * where \f$\epsilon^{ijk}\f$ is the spatial Levi-Civita symbol,
 * \f$K_{ij}\f$
 * is the extrinsic curvature, \f$\gamma_{jm} \f$ is the spatial metric,
 * and \f$D_i\f$ is spatial covariant derivative.
 */
template <typename Frame, typename DataType>
tnsr::ii<DataType, 3, Frame> weyl_magnetic(
    const tnsr::ijj<DataType, 3, Frame>& grad_extrinsic_curvature,
    const tnsr::ii<DataType, 3, Frame>& spatial_metric,
    const Scalar<DataType>& sqrt_det_spatial_metric);

template <typename Frame, typename DataType>
void weyl_magnetic(
    gsl::not_null<tnsr::ii<DataType, 3, Frame>*> weyl_magnetic_part,
    const tnsr::ijj<DataType, 3, Frame>& grad_extrinsic_curvature,
    const tnsr::ii<DataType, 3, Frame>& spatial_metric,
    const Scalar<DataType>& sqrt_det_spatial_metric);
/// @}

namespace Tags {
/// Compute item for the magnetic part of the weyl tensor in vacuum
/// Computed from the `ExtrinsicCurvature` and `SpatialMetric`
///
/// Can be retrieved using gr::Tags::WeylMagnetic
template <typename Frame, typename DataType>
struct WeylMagneticCompute : WeylMagnetic<Frame, DataType>, db::ComputeTag {
  using argument_tags =
      tmpl::list<::Tags::deriv<gr::Tags::ExtrinsicCurvature<3, Frame, DataType>,
                               tmpl::size_t<3>, Frame>,
                 gr::Tags::SpatialMetric<3, Frame, DataType>,
                 gr::Tags::SqrtDetSpatialMetric<DataType>>;

  using return_type = tnsr::ii<DataType, 3, Frame>;

  static constexpr auto function = static_cast<void (*)(
      gsl::not_null<tnsr::ii<DataType, 3, Frame>*>,
      const tnsr::ijj<DataType, 3, Frame>&, const tnsr::ii<DataType, 3, Frame>&,
      const Scalar<DataType>&)>(&weyl_magnetic<Frame, DataType>);

  using base = WeylMagnetic<Frame, DataType>;
};
}  // namespace Tags
}  // namespace gr
