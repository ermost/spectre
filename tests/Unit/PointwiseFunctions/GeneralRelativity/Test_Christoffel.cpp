// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <utility>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/DeterminantAndInverse.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Framework/CheckWithRandomValues.hpp"
#include "Framework/SetupLocalPythonEnvironment.hpp"
#include "Framework/TestHelpers.hpp"
#include "Helpers/DataStructures/DataBox/TestHelpers.hpp"
#include "Helpers/DataStructures/MakeWithRandomValues.hpp"
#include "NumericalAlgorithms/LinearOperators/PartialDerivatives.hpp"
#include "PointwiseFunctions/GeneralRelativity/Christoffel.hpp"
#include "PointwiseFunctions/GeneralRelativity/DerivativesOfSpacetimeMetric.hpp"
#include "PointwiseFunctions/GeneralRelativity/IndexManipulation.hpp"
#include "PointwiseFunctions/GeneralRelativity/InverseSpacetimeMetric.hpp"
#include "PointwiseFunctions/GeneralRelativity/Lapse.hpp"
#include "PointwiseFunctions/GeneralRelativity/Shift.hpp"
#include "PointwiseFunctions/GeneralRelativity/SpatialMetric.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

namespace Tags {
template <typename Tag, typename Dim, typename Frame, typename>
struct deriv;
}  // namespace Tags
template <typename X, typename Symm, typename IndexList>
class Tensor;

namespace {
template <size_t Dim, IndexType Index, typename DataType>
void test_christoffel(const DataType& used_for_size) {
  tnsr::abb<DataType, Dim, Frame::Inertial, Index> (*f)(
      const tnsr::abb<DataType, Dim, Frame::Inertial, Index>&) =
      &gr::christoffel_first_kind<Dim, Frame::Inertial, Index, DataType>;
  pypp::check_with_random_values<1>(f, "Christoffel", "christoffel_first_kind",
                                    {{{-10., 10.}}}, used_for_size);
}
}  // namespace

SPECTRE_TEST_CASE("Unit.PointwiseFunctions.GeneralRelativity.Christoffel",
                  "[PointwiseFunctions][Unit]") {
  pypp::SetupLocalPythonEnvironment local_python_env(
      "PointwiseFunctions/GeneralRelativity/");
  const DataVector dv(5);
  test_christoffel<1, IndexType::Spatial>(dv);
  test_christoffel<2, IndexType::Spatial>(dv);
  test_christoffel<3, IndexType::Spatial>(dv);
  test_christoffel<1, IndexType::Spacetime>(dv);
  test_christoffel<2, IndexType::Spacetime>(dv);
  test_christoffel<3, IndexType::Spacetime>(dv);
  test_christoffel<1, IndexType::Spatial>(0.);
  test_christoffel<2, IndexType::Spatial>(0.);
  test_christoffel<3, IndexType::Spatial>(0.);
  test_christoffel<1, IndexType::Spacetime>(0.);
  test_christoffel<2, IndexType::Spacetime>(0.);
  test_christoffel<3, IndexType::Spacetime>(0.);

  // Check that compute items work correctly in the DataBox
  // First, check that the names are correct
  TestHelpers::db::test_compute_tag<
      gr::Tags::SpatialChristoffelFirstKindCompute<DataVector, 3,
                                                   Frame::Inertial>>(
      "SpatialChristoffelFirstKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::TraceSpatialChristoffelFirstKindCompute<DataVector, 3,
                                                        Frame::Inertial>>(
      "TraceSpatialChristoffelFirstKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::SpatialChristoffelSecondKindCompute<DataVector, 3,
                                                    Frame::Inertial>>(
      "SpatialChristoffelSecondKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::TraceSpatialChristoffelSecondKindCompute<DataVector, 3,
                                                         Frame::Inertial>>(
      "TraceSpatialChristoffelSecondKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::SpacetimeChristoffelFirstKindCompute<DataVector, 3,
                                                     Frame::Inertial>>(
      "SpacetimeChristoffelFirstKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::TraceSpacetimeChristoffelFirstKindCompute<DataVector, 3,
                                                          Frame::Inertial>>(
      "TraceSpacetimeChristoffelFirstKind");
  TestHelpers::db::test_compute_tag<
      gr::Tags::SpacetimeChristoffelSecondKindCompute<DataVector, 3,
                                                      Frame::Inertial>>(
      "SpacetimeChristoffelSecondKind");

  // Check that the compute items return correct values
  const DataVector used_for_size{3., 4., 5.};
  MAKE_GENERATOR(generator);
  std::uniform_real_distribution<> distribution(-0.2, 0.2);

  const auto spacetime_metric = [&]() {
    auto spacetime_metric_l =
        make_with_random_values<tnsr::aa<DataVector, 3, Frame::Inertial>>(
            make_not_null(&generator), make_not_null(&distribution),
            used_for_size);
    // Make sure spacetime_metric isn't singular
    get<0, 0>(spacetime_metric_l) += -1.;
    for (size_t i = 1; i <= 3; ++i) {
      spacetime_metric_l.get(i, i) += 1.;
    }
    return spacetime_metric_l;
  }();

  const auto deriv_spatial_metric =
      make_with_random_values<tnsr::ijj<DataVector, 3, Frame::Inertial>>(
          make_not_null(&generator), make_not_null(&distribution),
          used_for_size);
  const auto deriv_shift =
      make_with_random_values<tnsr::iJ<DataVector, 3, Frame::Inertial>>(
          make_not_null(&generator), make_not_null(&distribution),
          used_for_size);
  const auto deriv_lapse =
      make_with_random_values<tnsr::i<DataVector, 3, Frame::Inertial>>(
          make_not_null(&generator), make_not_null(&distribution),
          used_for_size);

  const auto dt_spatial_metric =
      make_with_random_values<tnsr::ii<DataVector, 3, Frame::Inertial>>(
          make_not_null(&generator), make_not_null(&distribution),
          used_for_size);
  const auto dt_shift =
      make_with_random_values<tnsr::I<DataVector, 3, Frame::Inertial>>(
          make_not_null(&generator), make_not_null(&distribution),
          used_for_size);
  const auto dt_lapse = make_with_random_values<Scalar<DataVector>>(
      make_not_null(&generator), make_not_null(&distribution), used_for_size);

  const auto spatial_metric = gr::spatial_metric(spacetime_metric);
  const auto det_and_inverse_spatial_metric =
      determinant_and_inverse(spatial_metric);
  const auto& inverse_spatial_metric = det_and_inverse_spatial_metric.second;
  const auto shift = gr::shift(spacetime_metric, inverse_spatial_metric);
  const auto lapse = gr::lapse(shift, spacetime_metric);
  const auto inverse_spacetime_metric =
      gr::inverse_spacetime_metric(lapse, shift, inverse_spatial_metric);

  const auto derivatives_of_spacetime_metric =
      gr::derivatives_of_spacetime_metric(
          lapse, dt_lapse, deriv_lapse, shift, dt_shift, deriv_shift,
          spatial_metric, dt_spatial_metric, deriv_spatial_metric);

  const auto expected_spatial_christoffel_first_kind =
      gr::christoffel_first_kind(deriv_spatial_metric);
  const auto expected_trace_spatial_christoffel_first_kind = trace_last_indices(
      expected_spatial_christoffel_first_kind, inverse_spatial_metric);
  const auto expected_spatial_christoffel_second_kind =
      raise_or_lower_first_index(expected_spatial_christoffel_first_kind,
                                 inverse_spatial_metric);
  const auto expected_trace_spatial_christoffel_second_kind =
      trace_last_indices(expected_spatial_christoffel_second_kind,
                         inverse_spatial_metric);

  const auto expected_spacetime_christoffel_first_kind =
      gr::christoffel_first_kind(derivatives_of_spacetime_metric);
  const auto expected_trace_spacetime_christoffel_first_kind =
      trace_last_indices(expected_spacetime_christoffel_first_kind,
                         inverse_spacetime_metric);
  const auto expected_spacetime_christoffel_second_kind =
      raise_or_lower_first_index(expected_spacetime_christoffel_first_kind,
                                 inverse_spacetime_metric);

  // Compute Christoffel symbol of the 2nd kind in two different ways
  // (the direct one, which we are testing here, and the one already
  // tested independently, which uses christoffel_first_kind) and make
  // sure we get the same result.
  const auto spatial_christoffel_second_kind_test =
      gr::christoffel_second_kind(deriv_spatial_metric, inverse_spatial_metric);
  CHECK_ITERABLE_APPROX(expected_spatial_christoffel_second_kind,
                        spatial_christoffel_second_kind_test);
  const auto spacetime_christoffel_second_kind_test =
      gr::christoffel_second_kind(derivatives_of_spacetime_metric,
                                  inverse_spacetime_metric);
  CHECK_ITERABLE_APPROX(expected_spacetime_christoffel_second_kind,
                        spacetime_christoffel_second_kind_test);

  const auto box = db::create<
      db::AddSimpleTags<gr::Tags::InverseSpatialMetric<DataVector, 3>,
                        ::Tags::deriv<gr::Tags::SpatialMetric<DataVector, 3>,
                                      tmpl::size_t<3>, Frame::Inertial>,
                        gr::Tags::InverseSpacetimeMetric<DataVector, 3>,
                        gr::Tags::DerivativesOfSpacetimeMetric<DataVector, 3>>,
      db::AddComputeTags<
          gr::Tags::SpatialChristoffelFirstKindCompute<DataVector, 3,
                                                       Frame::Inertial>,
          gr::Tags::TraceSpatialChristoffelFirstKindCompute<DataVector, 3,
                                                            Frame::Inertial>,
          gr::Tags::SpatialChristoffelSecondKindCompute<DataVector, 3,
                                                        Frame::Inertial>,
          gr::Tags::TraceSpatialChristoffelSecondKindCompute<DataVector, 3,
                                                             Frame::Inertial>,
          gr::Tags::SpacetimeChristoffelFirstKindCompute<DataVector, 3,
                                                         Frame::Inertial>,
          gr::Tags::TraceSpacetimeChristoffelFirstKindCompute<DataVector, 3,
                                                              Frame::Inertial>,
          gr::Tags::SpacetimeChristoffelSecondKindCompute<DataVector, 3,
                                                          Frame::Inertial>>>(
      inverse_spatial_metric, deriv_spatial_metric, inverse_spacetime_metric,
      derivatives_of_spacetime_metric);

  CHECK(db::get<gr::Tags::SpatialChristoffelFirstKind<DataVector, 3>>(box) ==
        expected_spatial_christoffel_first_kind);
  CHECK(db::get<gr::Tags::TraceSpatialChristoffelFirstKind<DataVector, 3>>(
            box) == expected_trace_spatial_christoffel_first_kind);
  CHECK(db::get<gr::Tags::SpatialChristoffelSecondKind<DataVector, 3>>(box) ==
        expected_spatial_christoffel_second_kind);
  CHECK(db::get<gr::Tags::TraceSpatialChristoffelSecondKind<DataVector, 3>>(
            box) == expected_trace_spatial_christoffel_second_kind);

  CHECK(db::get<gr::Tags::SpacetimeChristoffelFirstKind<DataVector, 3>>(box) ==
        expected_spacetime_christoffel_first_kind);
  CHECK(db::get<gr::Tags::TraceSpacetimeChristoffelFirstKind<DataVector, 3>>(
            box) == expected_trace_spacetime_christoffel_first_kind);
  CHECK(db::get<gr::Tags::SpacetimeChristoffelSecondKind<DataVector, 3>>(box) ==
        expected_spacetime_christoffel_second_kind);
}
