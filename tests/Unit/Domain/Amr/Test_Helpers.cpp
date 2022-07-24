// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <array>
#include <cstddef>

#include "Domain/Amr/Flag.hpp"
#include "Domain/Amr/Helpers.hpp"
#include "Domain/Structure/Direction.hpp"
#include "Domain/Structure/ElementId.hpp"
#include "Domain/Structure/OrientationMap.hpp"
#include "Domain/Structure/SegmentId.hpp"
#include "Helpers/Domain/CoordinateMaps/TestMapHelpers.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/Gsl.hpp"

namespace {
void test_desired_refinement_levels() {
  ElementId<1> element_id_1d{0, {{SegmentId(2, 3)}}};
  CHECK(amr::domain::desired_refinement_levels(element_id_1d,
                                               {{amr::domain::Flag::Split}}) ==
        std::array<size_t, 1>{{3}});
  CHECK(amr::domain::desired_refinement_levels(
            element_id_1d, {{amr::domain::Flag::DoNothing}}) ==
        std::array<size_t, 1>{{2}});
  CHECK(amr::domain::desired_refinement_levels(element_id_1d,
                                               {{amr::domain::Flag::Join}}) ==
        std::array<size_t, 1>{{1}});

  ElementId<2> element_id_2d{1, {{SegmentId(3, 5), SegmentId(1, 1)}}};
  CHECK(amr::domain::desired_refinement_levels(
            element_id_2d,
            {{amr::domain::Flag::Split, amr::domain::Flag::Join}}) ==
        std::array<size_t, 2>{{4, 0}});
  CHECK(amr::domain::desired_refinement_levels(
            element_id_2d,
            {{amr::domain::Flag::Join, amr::domain::Flag::DoNothing}}) ==
        std::array<size_t, 2>{{2, 1}});
  CHECK(amr::domain::desired_refinement_levels(
            element_id_2d,
            {{amr::domain::Flag::Join, amr::domain::Flag::Join}}) ==
        std::array<size_t, 2>{{2, 0}});
  CHECK(amr::domain::desired_refinement_levels(
            element_id_2d,
            {{amr::domain::Flag::DoNothing, amr::domain::Flag::Split}}) ==
        std::array<size_t, 2>{{3, 2}});
  CHECK(amr::domain::desired_refinement_levels(
            element_id_2d,
            {{amr::domain::Flag::DoNothing, amr::domain::Flag::DoNothing}}) ==
        std::array<size_t, 2>{{3, 1}});

  ElementId<3> element_id_3d{
      7, {{SegmentId(5, 15), SegmentId(2, 0), SegmentId(4, 6)}}};
  CHECK(amr::domain::desired_refinement_levels(
            element_id_3d, {{amr::domain::Flag::Split, amr::domain::Flag::Join,
                             amr::domain::Flag::DoNothing}}) ==
        std::array<size_t, 3>{{6, 1, 4}});
}

template <size_t VolumeDim>
void check_desired_refinement_levels_of_neighbor(
    const ElementId<VolumeDim>& neighbor_id,
    const std::array<amr::domain::Flag, VolumeDim>& neighbor_flags) {
  for (OrientationMapIterator<VolumeDim> orientation{}; orientation;
       ++orientation) {
    const auto desired_levels_my_frame = desired_refinement_levels_of_neighbor(
        neighbor_id, neighbor_flags, *orientation);
    const auto desired_levels_neighbor_frame =
        desired_refinement_levels(neighbor_id, neighbor_flags);
    for (size_t d = 0; d < VolumeDim; ++d) {
      CHECK(gsl::at(desired_levels_my_frame, d) ==
            gsl::at(desired_levels_neighbor_frame, (*orientation)(d)));
    }
  }
}

void test_desired_refinement_levels_of_neighbor() {
  ElementId<1> neighbor_id_1d{0, {{SegmentId(2, 3)}}};
  check_desired_refinement_levels_of_neighbor(neighbor_id_1d,
                                              {{amr::domain::Flag::Split}});
  check_desired_refinement_levels_of_neighbor(neighbor_id_1d,
                                              {{amr::domain::Flag::DoNothing}});
  check_desired_refinement_levels_of_neighbor(neighbor_id_1d,
                                              {{amr::domain::Flag::Join}});

  ElementId<2> neighbor_id_2d{1, {{SegmentId(3, 0), SegmentId(1, 1)}}};
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_2d, {{amr::domain::Flag::Split, amr::domain::Flag::Join}});
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_2d,
      {{amr::domain::Flag::Join, amr::domain::Flag::DoNothing}});
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_2d, {{amr::domain::Flag::Join, amr::domain::Flag::Join}});
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_2d,
      {{amr::domain::Flag::DoNothing, amr::domain::Flag::Split}});
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_2d,
      {{amr::domain::Flag::DoNothing, amr::domain::Flag::DoNothing}});

  ElementId<3> neighbor_id_3d{
      7, {{SegmentId(5, 31), SegmentId(2, 0), SegmentId(4, 15)}}};
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_3d, {{amr::domain::Flag::Split, amr::domain::Flag::Join,
                        amr::domain::Flag::DoNothing}});
  check_desired_refinement_levels_of_neighbor(
      neighbor_id_3d, {{amr::domain::Flag::Join, amr::domain::Flag::DoNothing,
                        amr::domain::Flag::Split}});
}

void test_has_potential_sibling() {
  ElementId<1> element_id_1d{0, {{SegmentId(2, 3)}}};
  CHECK(amr::domain::has_potential_sibling(element_id_1d,
                                           Direction<1>::lower_xi()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_1d,
                                                 Direction<1>::upper_xi()));

  ElementId<2> element_id_2d{0, {{SegmentId(3, 0), SegmentId{1, 1}}}};
  CHECK(amr::domain::has_potential_sibling(element_id_2d,
                                           Direction<2>::upper_xi()));
  CHECK(amr::domain::has_potential_sibling(element_id_2d,
                                           Direction<2>::lower_eta()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_2d,
                                                 Direction<2>::lower_xi()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_2d,
                                                 Direction<2>::upper_eta()));

  ElementId<3> element_id_3d{
      7, {{SegmentId(5, 31), SegmentId(2, 0), SegmentId(4, 15)}}};
  CHECK(amr::domain::has_potential_sibling(element_id_3d,
                                           Direction<3>::lower_xi()));
  CHECK(amr::domain::has_potential_sibling(element_id_3d,
                                           Direction<3>::upper_eta()));
  CHECK(amr::domain::has_potential_sibling(element_id_3d,
                                           Direction<3>::lower_zeta()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_3d,
                                                 Direction<3>::upper_xi()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_3d,
                                                 Direction<3>::lower_eta()));
  CHECK_FALSE(amr::domain::has_potential_sibling(element_id_3d,
                                                 Direction<3>::upper_zeta()));
}
}  // namespace

SPECTRE_TEST_CASE("Unit.Domain.Amr.Helpers", "[Domain][Unit]") {
  test_desired_refinement_levels();
  test_desired_refinement_levels_of_neighbor();
  test_has_potential_sibling();
}

// [[OutputRegex, Undefined Flag in dimension]]
[[noreturn]] SPECTRE_TEST_CASE("Unit.Domain.Amr.Helpers.BadFlag",
                               "[Domain][Unit]") {
  ASSERTION_TEST();
#ifdef SPECTRE_DEBUG
  auto bad_flag = amr::domain::desired_refinement_levels(
      ElementId<1>{0}, {{amr::domain::Flag::Undefined}});
  static_cast<void>(bad_flag);
  ERROR("Failed to trigger ASSERT in an assertion test");
#endif
}

// [[OutputRegex, Undefined Flag in dimension]]
[[noreturn]] SPECTRE_TEST_CASE("Unit.Domain.Amr.Helpers.BadFlag2",
                               "[Domain][Unit]") {
  ASSERTION_TEST();
#ifdef SPECTRE_DEBUG
  auto bad_flag = amr::domain::desired_refinement_levels_of_neighbor(
      ElementId<1>{0}, {{amr::domain::Flag::Undefined}},
      OrientationMap<1>{{{Direction<1>::lower_xi()}}});
  static_cast<void>(bad_flag);
  ERROR("Failed to trigger ASSERT in an assertion test");
#endif
}
