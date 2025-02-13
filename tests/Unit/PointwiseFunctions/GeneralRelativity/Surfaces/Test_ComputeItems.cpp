// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <cstddef>
#include <random>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/DeterminantAndInverse.hpp"
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Evolution/Systems/GeneralizedHarmonic/Tags.hpp"
#include "Framework/TestHelpers.hpp"
#include "Helpers/DataStructures/DataBox/TestHelpers.hpp"
#include "Helpers/DataStructures/MakeWithRandomValues.hpp"
#include "PointwiseFunctions/GeneralRelativity/Christoffel.hpp"
#include "PointwiseFunctions/GeneralRelativity/ExtrinsicCurvature.hpp"
#include "PointwiseFunctions/GeneralRelativity/GeneralizedHarmonic/Phi.hpp"
#include "PointwiseFunctions/GeneralRelativity/GeneralizedHarmonic/Pi.hpp"
#include "PointwiseFunctions/GeneralRelativity/IndexManipulation.hpp"
#include "PointwiseFunctions/GeneralRelativity/SpacetimeMetric.hpp"
#include "PointwiseFunctions/GeneralRelativity/Surfaces/ComputeItems.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

namespace {
template <size_t Dim, typename Frame, typename T>
void test_strahlkorper_compute_items(const T& used_for_size) {
  // Set up random values for lapse, shift, spatial_metric,
  // and their derivatives.
  MAKE_GENERATOR(generator);
  std::uniform_real_distribution<> dist(-1., 1.);
  std::uniform_real_distribution<> dist_positive(1., 2.);
  const auto nn_generator = make_not_null(&generator);
  const auto nn_dist = make_not_null(&dist);
  const auto nn_dist_positive = make_not_null(&dist_positive);

  const auto lapse = make_with_random_values<Scalar<T>>(
      nn_generator, nn_dist_positive, used_for_size);
  const auto shift = make_with_random_values<tnsr::I<T, Dim>>(
      nn_generator, nn_dist, used_for_size);
  const auto spatial_metric = [&]() {
    auto spatial_metric_l = make_with_random_values<tnsr::ii<T, Dim>>(
        nn_generator, nn_dist, used_for_size);
    // Make sure spatial_metric isn't singular by adding
    // large enough positive diagonal values.
    for (size_t i = 0; i < Dim; ++i) {
      spatial_metric_l.get(i, i) += 4.0;
    }
    return spatial_metric_l;
  }();
  const auto dt_lapse = make_with_random_values<Scalar<T>>(
      nn_generator, nn_dist_positive, used_for_size);
  const auto deriv_lapse = make_with_random_values<tnsr::i<T, Dim>>(
      nn_generator, nn_dist_positive, used_for_size);
  const auto dt_shift = make_with_random_values<tnsr::I<T, Dim>>(
      nn_generator, nn_dist, used_for_size);
  const auto deriv_shift = make_with_random_values<tnsr::iJ<T, Dim>>(
      nn_generator, nn_dist, used_for_size);
  const auto deriv_spatial_metric = make_with_random_values<tnsr::ijj<T, Dim>>(
      nn_generator, nn_dist, used_for_size);
  const auto dt_spatial_metric = make_with_random_values<tnsr::ii<T, Dim>>(
      nn_generator, nn_dist, used_for_size);

  // Make spacetime metric, extrinsic curvature, inverse spatial
  // metric, spatial christoffel of the second kind, and generalized
  // harmonic pi, psi variables in a way that is already independently
  // tested.
  const auto spacetime_metric =
      gr::spacetime_metric(lapse, shift, spatial_metric);
  const auto expected_extrinsic_curvature =
      gr::extrinsic_curvature(lapse, shift, deriv_shift, spatial_metric,
                              dt_spatial_metric, deriv_spatial_metric);
  const auto expected_inverse_spatial_metric =
      determinant_and_inverse(spatial_metric).second;
  const auto expected_spatial_christoffel_second_kind =
      raise_or_lower_first_index(
          gr::christoffel_first_kind(deriv_spatial_metric),
          expected_inverse_spatial_metric);
  const auto phi = gh::phi(lapse, deriv_lapse, shift, deriv_shift,
                           spatial_metric, deriv_spatial_metric);
  const auto pi = gh::pi(lapse, dt_lapse, shift, dt_shift, spatial_metric,
                         dt_spatial_metric, phi);

  // Now test the ComputeItems.
  const auto box = db::create<
      db::AddSimpleTags<
          tmpl::list<gr::Tags::SpacetimeMetric<DataVector, Dim, Frame>,
                     gh::Tags::Pi<DataVector, Dim, Frame>,
                     gh::Tags::Phi<DataVector, Dim, Frame>>>,
      db::AddComputeTags<tmpl::list<
          gr::surfaces::Tags::InverseSpatialMetricCompute<Dim, Frame>,
          gr::surfaces::Tags::ExtrinsicCurvatureCompute<Dim, Frame>,
          gr::surfaces::Tags::SpatialChristoffelSecondKindCompute<Dim,
                                                                  Frame>>>>(
      spacetime_metric, pi, phi);

  const auto& inverse_spatial_metric =
      db::get<gr::Tags::InverseSpatialMetric<DataVector, Dim, Frame>>(box);
  const auto& extrinsic_curvature =
      db::get<gr::Tags::ExtrinsicCurvature<DataVector, Dim, Frame>>(box);
  const auto& spatial_christoffel_second_kind =
      db::get<gr::Tags::SpatialChristoffelSecondKind<DataVector, Dim, Frame>>(
          box);
  CHECK_ITERABLE_APPROX(inverse_spatial_metric,
                        expected_inverse_spatial_metric);
  CHECK_ITERABLE_APPROX(extrinsic_curvature, expected_extrinsic_curvature);
  CHECK_ITERABLE_APPROX(spatial_christoffel_second_kind,
                        expected_spatial_christoffel_second_kind);
}
}  // namespace

SPECTRE_TEST_CASE(
    "Unit.PointwiseFunctions.GeneralRelativity.Surfaces.ComputeItems",
    "[PointwiseFunctions][Unit]") {
  const DataVector used_for_size(20);
  // Need only Dim=3 and DataVectors for apparent horizons.
  test_strahlkorper_compute_items<3, Frame::Inertial>(used_for_size);
  TestHelpers::db::test_compute_tag<
      gr::surfaces::Tags::InverseSpatialMetricCompute<3, Frame::Inertial>>(
      "InverseSpatialMetric");
  TestHelpers::db::test_compute_tag<
      gr::surfaces::Tags::ExtrinsicCurvatureCompute<3, Frame::Inertial>>(
      "ExtrinsicCurvature");
  TestHelpers::db::test_compute_tag<
      gr::surfaces::Tags::SpatialChristoffelSecondKindCompute<3,
                                                              Frame::Inertial>>(
      "SpatialChristoffelSecondKind");
}
