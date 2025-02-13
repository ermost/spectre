// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>
#include <pup.h>

#include "DataStructures/DataBox/DataBox.hpp"
#include "Domain/Creators/Tags/Domain.hpp"
#include "Domain/ElementLogicalCoordinates.hpp"
#include "Domain/FunctionsOfTime/FunctionOfTime.hpp"
#include "Domain/Structure/ElementId.hpp"
#include "NumericalAlgorithms/Interpolation/IrregularInterpolant.hpp"
#include "Options/String.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "ParallelAlgorithms/EventsAndTriggers/Event.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InterpolationTargetVarsFromElement.hpp"
#include "ParallelAlgorithms/Interpolation/Events/GetComputeItemsOnSource.hpp"
#include "ParallelAlgorithms/Interpolation/PointInfoTag.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"
#include "Utilities/PrettyType.hpp"
#include "Utilities/Serialization/CharmPupable.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TypeTraits/CreateGetStaticMemberVariableOrDefault.hpp"

/// \cond
namespace Events::Tags {
template <size_t Dim>
struct ObserverMesh;
}  // namespace Events::Tags
namespace Tags {
template <typename TagsList>
struct Variables;
}  // namespace Tags
template <size_t VolumeDim>
class ElementId;
namespace intrp {
template <typename Metavariables, typename Tag>
struct InterpolationTarget;
}  // namespace intrp
/// \endcond

namespace intrp::Events {
/// \cond
template <size_t VolumeDim, typename InterpolationTargetTag,
          typename SourceVarTags>
class InterpolateWithoutInterpComponent;
/// \endcond

/// Does an interpolation onto an InterpolationTargetTag by calling Actions on
/// the InterpolationTarget component.
template <size_t VolumeDim, typename InterpolationTargetTag,
          typename... SourceVarTags>
class InterpolateWithoutInterpComponent<VolumeDim, InterpolationTargetTag,
                                        tmpl::list<SourceVarTags...>>
    : public Event {
  /// \cond
  explicit InterpolateWithoutInterpComponent(CkMigrateMessage* /*unused*/) {}
  using PUP::able::register_constructor;
  WRAPPED_PUPable_decl_template(InterpolateWithoutInterpComponent);  // NOLINT
  /// \endcond

  using options = tmpl::list<>;
  static constexpr Options::String help =
      "Does interpolation using the given InterpolationTargetTag, "
      "without an Interpolator ParallelComponent.";

  static std::string name() {
    return pretty_type::name<InterpolationTargetTag>();
  }

  InterpolateWithoutInterpComponent() = default;

  using compute_tags_for_observation_box =
      detail::get_compute_items_on_source_or_default_t<InterpolationTargetTag,
                                                       tmpl::list<>>;

  using argument_tags =
      tmpl::list<typename InterpolationTargetTag::temporal_id,
                 Tags::InterpPointInfoBase,
                 ::Events::Tags::ObserverMesh<VolumeDim>, SourceVarTags...>;

  template <typename ParallelComponent, typename Metavariables>
  void operator()(
      const typename InterpolationTargetTag::temporal_id::type& temporal_id,
      const typename Tags::InterpPointInfo<Metavariables>::type& point_infos,
      const Mesh<VolumeDim>& mesh,
      const typename SourceVarTags::type&... source_vars_input,
      Parallel::GlobalCache<Metavariables>& cache,
      const ElementId<VolumeDim>& array_index,
      const ParallelComponent* const /*meta*/,
      const ObservationValue& /*observation_value*/) const {
    const auto& block_logical_coords =
        InterpolationTarget_detail::block_logical_coords<
            InterpolationTargetTag>(
            cache,
            get<Vars::PointInfoTag<InterpolationTargetTag, VolumeDim>>(
                point_infos),
            temporal_id);
    const std::vector<ElementId<VolumeDim>> element_ids{{array_index}};
    const auto element_coord_holders =
        element_logical_coordinates(element_ids, block_logical_coords);

    if (element_coord_holders.count(array_index) == 0) {
      // There are no target points in this element, so we don't need
      // to do anything.
      return;
    }

    // There are points in this element, so interpolate to them and
    // send the interpolated data to the target.  This is done
    // in several steps:
    const auto& element_coord_holder = element_coord_holders.at(array_index);

    // 1. Get the list of variables
    Variables<typename InterpolationTargetTag::vars_to_interpolate_to_target>
        interp_vars(mesh.number_of_grid_points());

    if constexpr (InterpolationTarget_detail::has_compute_vars_to_interpolate_v<
                      InterpolationTargetTag>) {
      // 1a. Call compute_vars_to_interpolate.  Need the source in a
      // Variables, so copy the variables here.
      // This copy would be unnecessary if we passed a Variables into
      // InterpolateWithoutInterpComponent instead of passing
      // individual Tensors, which would require that this Variables is
      // something in the DataBox. (Note that
      // InterpolationTarget_detail::compute_dest_vars_from_source_vars
      // allows the source variables to be different from the
      // destination variables).
      Variables<tmpl::list<SourceVarTags...>> source_vars(
          mesh.number_of_grid_points());
      [[maybe_unused]] const auto copy_to_variables =
          [&source_vars](const auto source_var_tag_v, const auto& source_var) {
            using source_var_tag = tmpl::type_from<decltype(source_var_tag_v)>;
            get<source_var_tag>(source_vars) = source_var;
            return 0;
          };
      expand_pack(copy_to_variables(tmpl::type_<SourceVarTags>{},
                                    source_vars_input)...);

      InterpolationTarget_detail::compute_dest_vars_from_source_vars<
          InterpolationTargetTag>(make_not_null(&interp_vars), source_vars,
                                  get<domain::Tags::Domain<VolumeDim>>(cache),
                                  mesh, array_index, cache, temporal_id);
    } else {
      // 1b. There is no compute_vars_to_interpolate. So copy the
      // source vars directly into the variables.
      // This copy would be unnecessary if:
      //   - We passed a Variables into InterpolateWithoutInterpComponent
      //     instead of passing individual Tensors.
      //  and
      //   - This Variables was actually something in the DataBox.
      //  and
      //   - Either the passed-in Variables was exactly the same as
      //     InterpolationTargetTag::vars_to_interpolate_to_target,
      //     or IrregularInterpolant::interpolate had the ability to
      //     interpolate only a subset of the Variables passed into it,
      //     or IrregularInterpolant::interpolate can interpolate individual
      //     DataVectors.
      [[maybe_unused]] const auto copy_to_variables =
          [&interp_vars](const auto tensor_tag_v, const auto& tensor) {
            using tensor_tag = tmpl::type_from<decltype(tensor_tag_v)>;
            get<tensor_tag>(interp_vars) = tensor;
            return 0;
          };
      expand_pack(copy_to_variables(tmpl::type_<SourceVarTags>{},
                                    source_vars_input)...);
    }

    // 2. Set up interpolator
    intrp::Irregular<VolumeDim> interpolator(
        mesh, element_coord_holder.element_logical_coords);

    // 3. Interpolate and send interpolated data to target
    auto& receiver_proxy = Parallel::get_parallel_component<
        InterpolationTarget<Metavariables, InterpolationTargetTag>>(cache);
    Parallel::simple_action<
        Actions::InterpolationTargetVarsFromElement<InterpolationTargetTag>>(
        receiver_proxy,
        std::vector<Variables<
            typename InterpolationTargetTag::vars_to_interpolate_to_target>>(
            {interpolator.interpolate(interp_vars)}),
        block_logical_coords,
        std::vector<std::vector<size_t>>({element_coord_holder.offsets}),
        temporal_id);
  }

  using is_ready_argument_tags = tmpl::list<>;

  template <typename ArrayIndex, typename Component, typename Metavariables>
  bool is_ready(Parallel::GlobalCache<Metavariables>& /*cache*/,
                const ArrayIndex& /*array_index*/,
                const Component* const /*meta*/) const {
    return true;
  }

  bool needs_evolved_variables() const override { return true; }
};

/// \cond
template <size_t VolumeDim, typename InterpolationTargetTag,
          typename... SourceVarTags>
PUP::able::PUP_ID
    InterpolateWithoutInterpComponent<VolumeDim, InterpolationTargetTag,
                                      tmpl::list<SourceVarTags...>>::my_PUP_ID =
        0;  // NOLINT
/// \endcond

}  // namespace intrp::Events
