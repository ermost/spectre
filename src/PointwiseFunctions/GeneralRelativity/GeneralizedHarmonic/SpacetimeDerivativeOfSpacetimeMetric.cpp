// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "PointwiseFunctions/GeneralRelativity/GeneralizedHarmonic/SpacetimeDerivativeOfSpacetimeMetric.hpp"

#include "DataStructures/DataVector.hpp"     // IWYU pragma: keep
#include "DataStructures/Tensor/Tensor.hpp"  // IWYU pragma: keep
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/ContainerHelpers.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"

// IWYU pragma: no_forward_declare Tensor

namespace GeneralizedHarmonic {
template <size_t SpatialDim, typename Frame, typename DataType>
void spacetime_derivative_of_spacetime_metric(
    const gsl::not_null<tnsr::abb<DataType, SpatialDim, Frame>*>
        da_spacetime_metric,
    const Scalar<DataType>& lapse,
    const tnsr::I<DataType, SpatialDim, Frame>& shift,
    const tnsr::aa<DataType, SpatialDim, Frame>& pi,
    const tnsr::iaa<DataType, SpatialDim, Frame>& phi) {
  destructive_resize_components(da_spacetime_metric, get_size(get(lapse)));
  for (size_t a = 0; a < SpatialDim + 1; ++a) {
    for (size_t b = a; b < SpatialDim + 1; ++b) {
      da_spacetime_metric->get(0, a, b) = -pi.get(a, b) * get(lapse);
      for (size_t i = 0; i < SpatialDim; ++i) {
        da_spacetime_metric->get(0, a, b) += shift.get(i) * phi.get(i, a, b);
        da_spacetime_metric->get(i + 1, a, b) = phi.get(i, a, b);
      }
    }
  }
}
}  // namespace GeneralizedHarmonic

#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)
#define DTYPE(data) BOOST_PP_TUPLE_ELEM(1, data)
#define FRAME(data) BOOST_PP_TUPLE_ELEM(2, data)

#define INSTANTIATE(_, data)                                                   \
  template void GeneralizedHarmonic::spacetime_derivative_of_spacetime_metric( \
      const gsl::not_null<tnsr::abb<DTYPE(data), DIM(data), FRAME(data)>*>     \
          dt_spacetime_metric,                                                 \
      const Scalar<DTYPE(data)>& lapse,                                        \
      const tnsr::I<DTYPE(data), DIM(data), FRAME(data)>& shift,               \
      const tnsr::aa<DTYPE(data), DIM(data), FRAME(data)>& pi,                 \
      const tnsr::iaa<DTYPE(data), DIM(data), FRAME(data)>& phi);

GENERATE_INSTANTIATIONS(INSTANTIATE, (1, 2, 3), (double, DataVector),
                        (Frame::Grid, Frame::Inertial))

#undef DIM
#undef DTYPE
#undef FRAME
#undef INSTANTIATE
