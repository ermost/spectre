// Distributed under the MIT License.
// See LICENSE.txt for details.

/// \file
/// Functions used for adaptive mesh refinement decisions.

#pragma once

#include <array>
#include <cstddef>

#include "Domain/Amr/Flag.hpp"

/// \cond
template <size_t VolumeDim>
class Direction;

template <size_t VolumeDim>
class ElementId;

template <size_t VolumeDim>
class OrientationMap;
/// \endcond

namespace amr::domain {
/// \ingroup AmrGroup
/// \brief Computes the desired refinement level of the Element with ElementId
/// `id` given the desired amr::Flag%s `flags`
template <size_t VolumeDim>
std::array<size_t, VolumeDim> desired_refinement_levels(
    const ElementId<VolumeDim>& id, const std::array<Flag, VolumeDim>& flags);

/// \ingroup AmrGroup
/// \brief Computes the desired refinement level of a neighboring Element with
/// ElementId `neighbor_id` given its desired amr::Flag%s `neighbor_flags`
/// taking into account the OrientationMap `orientation` of the neighbor
///
/// \details The OrientationMap `orientation` is that from the Element that has
/// a neighbor with ElementId `neighbor_id`
template <size_t VolumeDim>
std::array<size_t, VolumeDim> desired_refinement_levels_of_neighbor(
    const ElementId<VolumeDim>& neighbor_id,
    const std::array<Flag, VolumeDim>& neighbor_flags,
    const OrientationMap<VolumeDim>& orientation);

/// \ingroup AmrGroup
/// \brief Whether or not the Element with `element_id` can have a sibling
/// in the given `direction`
template <size_t VolumeDim>
bool has_potential_sibling(const ElementId<VolumeDim>& element_id,
                           const Direction<VolumeDim>& direction);
}  // namespace amr::domain
