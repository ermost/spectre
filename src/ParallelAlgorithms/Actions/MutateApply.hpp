// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <optional>
#include <tuple>

#include "DataStructures/DataBox/DataBox.hpp"
#include "Parallel/AlgorithmExecution.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace tuples {
template <typename...>
class TaggedTuple;
}  // namespace tuples

namespace Parallel {
template <typename Metavariables>
class GlobalCache;
}  // namespace Parallel
/// \endcond

namespace Actions {
/*!
 * \ingroup ActionsGroup
 * \brief Apply the function `Mutator::apply` to the DataBox
 *
 * The function `Mutator::apply` is invoked with the `Mutator::argument_tags`.
 * The result of this computation is stored in the `Mutator::return_tags`.
 *
 * Uses:
 * - DataBox:
 *   - All elements in `Mutator::argument_tags`
 *
 * DataBox changes:
 * - Modifies:
 *   - All elements in `Mutator::return_tags`
 */
template <typename Mutator>
struct MutateApply {
  template <typename DataBox, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      DataBox& box, const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      const Parallel::GlobalCache<Metavariables>& /*cache*/,
      const ArrayIndex& /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    db::mutate_apply<Mutator>(make_not_null(&box));
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};
}  // namespace Actions
