// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <cstddef>

#include "DataStructures/Index.hpp"
#include "Domain/Structure/Direction.hpp"
#include "Domain/Structure/DirectionMap.hpp"
#include "Framework/Pypp.hpp"
#include "Framework/SetupLocalPythonEnvironment.hpp"
#include "Helpers/NumericalAlgorithms/FiniteDifference/Exact.hpp"
#include "Helpers/NumericalAlgorithms/FiniteDifference/Python.hpp"
#include "NumericalAlgorithms/FiniteDifference/MonotonicityPreserving5.hpp"

namespace {

const double alpha = 4.0;
const double epsilon = 1e-10;

template <size_t Dim>
void test() {
  const auto recons =
      [](const gsl::not_null<std::array<gsl::span<double>, Dim>*>
             reconstructed_upper_side_of_face_vars,
         const gsl::not_null<std::array<gsl::span<double>, Dim>*>
             reconstructed_lower_side_of_face_vars,
         const gsl::span<const double>& volume_vars,
         const DirectionMap<Dim, gsl::span<const double>>& ghost_cell_vars,
         const Index<Dim>& volume_extents, const size_t number_of_variables) {
        fd::reconstruction::monotonicity_preserving_5(
            reconstructed_upper_side_of_face_vars,
            reconstructed_lower_side_of_face_vars, volume_vars, ghost_cell_vars,
            volume_extents, number_of_variables, alpha, epsilon);
      };
  const auto recons_neighbor_data =
      [](const gsl::not_null<DataVector*> face_data,
         const DataVector& volume_data, const DataVector& neighbor_data,
         const Index<Dim>& volume_extents, const Index<Dim>& ghost_data_extents,
         const Direction<Dim>& direction_to_reconstruct) {
        if (direction_to_reconstruct.side() == Side::Upper) {
          fd::reconstruction::reconstruct_neighbor<
              Side::Upper,
              fd::reconstruction::detail::MonotonicityPreserving5Reconstructor>(
              face_data, volume_data, neighbor_data, volume_extents,
              ghost_data_extents, direction_to_reconstruct, alpha, epsilon);
        }
        if (direction_to_reconstruct.side() == Side::Lower) {
          fd::reconstruction::reconstruct_neighbor<
              Side::Lower,
              fd::reconstruction::detail::MonotonicityPreserving5Reconstructor>(
              face_data, volume_data, neighbor_data, volume_extents,
              ghost_data_extents, direction_to_reconstruct, alpha, epsilon);
        }
      };
  TestHelpers::fd::reconstruction::test_reconstruction_is_exact_if_in_basis<
      Dim>(4, 5, 5, recons, recons_neighbor_data);
  TestHelpers::fd::reconstruction::test_with_python(
      Index<Dim>{5}, 5, "MonotonicityPreserving5", "test_mp5", recons,
      recons_neighbor_data);
}
}  // namespace

SPECTRE_TEST_CASE("Unit.FiniteDifference.MonotonicityPreserving5",
                  "[Unit][NumericalAlgorithms]") {
  pypp::SetupLocalPythonEnvironment local_python_env(
      "NumericalAlgorithms/FiniteDifference/");
  test<1>();
  test<2>();
  test<3>();
}
