// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>
#include <optional>
#include <tuple>

#include "DataStructures/DataBox/DataBox.hpp"
#include "Domain/Structure/ElementId.hpp"
#include "Domain/Tags.hpp"
#include "IO/Importers/Tags.hpp"
#include "IO/Observer/ArrayComponentId.hpp"
#include "Parallel/AlgorithmExecution.hpp"
#include "Parallel/ArrayIndex.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/Local.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Literals.hpp"
#include "Utilities/MakeString.hpp"
#include "Utilities/Requires.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

/// \cond
namespace importers {
template <typename Metavariables>
struct ElementDataReader;
namespace Actions {
struct RegisterElementWithSelf;
}  // namespace Actions
}  // namespace importers
namespace evolution::dg::subcell {
template <typename DgTag, typename SubcellTag, typename DbTagsList>
const typename DgTag::type& get_active_tag(const db::DataBox<DbTagsList>& box);
namespace Tags {
template <size_t Dim, typename Frame>
struct Coordinates;
struct ActiveGrid;
}  // namespace Tags
}  // namespace evolution::dg::subcell
/// \endcond

namespace importers::Actions {

/*!
 * \brief Register an element with the volume data reader component.
 *
 * Invoke this action on each element of an array parallel component to register
 * them for receiving imported volume data.
 *
 * \note If the tags `evolution::dg::subcell::Tags::ActiveGrid` and
 * `evolution::dg::subcell::Tags::Coordinates<Dim, Frame::Inertial>` are
 * retrievable from the DataBox, then interpolation to the FD/subcell grid is
 * possible.
 *
 * \see Dev guide on \ref dev_guide_importing
 */
struct RegisterWithElementDataReader {
  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            size_t Dim, typename ActionList, typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTagsList>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      Parallel::GlobalCache<Metavariables>& cache,
      const ElementId<Dim>& array_index, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    auto& local_reader_component = *Parallel::local_branch(
        Parallel::get_parallel_component<
            importers::ElementDataReader<Metavariables>>(cache));
    const auto& coords = [&box]() {
      if constexpr (db::tag_is_retrievable_v<
                        evolution::dg::subcell::Tags::ActiveGrid,
                        db::DataBox<DbTagsList>> and
                    db::tag_is_retrievable_v<
                        evolution::dg::subcell::Tags::Coordinates<
                            Dim, Frame::Inertial>,
                        db::DataBox<DbTagsList>>) {
        return evolution::dg::subcell::get_active_tag<
            domain::Tags::Coordinates<Dim, Frame::Inertial>,
            evolution::dg::subcell::Tags::Coordinates<Dim, Frame::Inertial>>(
            box);
      } else {
        return db::get<domain::Tags::Coordinates<Dim, Frame::Inertial>>(box);
      }
    }();
    Parallel::simple_action<importers::Actions::RegisterElementWithSelf>(
        local_reader_component,
        observers::ArrayComponentId(
            std::add_pointer_t<ParallelComponent>{nullptr},
            Parallel::ArrayIndex<ElementId<Dim>>(array_index)),
        coords);
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};

/*!
 * \brief Invoked on the `importers::ElementDataReader` component to store the
 * registered data.
 *
 * The `importers::Actions::RegisterWithElementDataReader` action, which is
 * performed on each element of an array parallel component, invokes this action
 * on the `importers::ElementDataReader` component.
 *
 * \see Dev guide on \ref dev_guide_importing
 */
struct RegisterElementWithSelf {
  template <typename ParallelComponent, typename DbTagsList,
            typename Metavariables, typename ArrayIndex, size_t Dim>
  static void apply(
      db::DataBox<DbTagsList>& box,
      const Parallel::GlobalCache<Metavariables>& /*cache*/,
      const ArrayIndex& /*array_index*/,
      const observers::ArrayComponentId& array_component_id,
      const tnsr::I<DataVector, Dim, Frame::Inertial>& inertial_coords) {
    db::mutate<Tags::RegisteredElements<Dim>>(
        [&array_component_id, &inertial_coords](
            const gsl::not_null<
                std::unordered_map<observers::ArrayComponentId,
                                   tnsr::I<DataVector, Dim, Frame::Inertial>>*>
                registered_elements) {
          (*registered_elements)[array_component_id] = inertial_coords;
        },
        make_not_null(&box));
  }
};

}  // namespace importers::Actions
