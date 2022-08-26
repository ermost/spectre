// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "DataStructures/Tensor/TypeAliases.hpp"

/// \cond
template <typename Frame>
class Strahlkorper;
template <size_t Dim>
class Domain;
namespace domain::FunctionsOfTime {
class FunctionOfTime;
}  // namespace domain::FunctionsOfTime
namespace gsl {
template <typename T>
class not_null;
}  // namespace gsl
/// \endcond

/// \brief Maps (cartesian) collocation points of a Strahlkorper to
/// a different frame.
///
/// Note that because the Blocks inside the Domain allow access to
/// maps only between a selected subset of frames, we cannot use
/// strahlkorper_in_different_frame to map between arbitrary frames;
/// allowing strahlkorper_coords_in_different_frame to work on more frames
/// requires adding member functions to Block.
template <typename SrcFrame, typename DestFrame>
void strahlkorper_coords_in_different_frame(
    gsl::not_null<tnsr::I<DataVector, 3, DestFrame>*> dest_cartesian_coords,
    const Strahlkorper<SrcFrame>& src_strahlkorper, const Domain<3>& domain,
    const std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
        functions_of_time,
    double time);
