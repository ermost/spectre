// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "DataStructures/DataBox/DataBox.hpp"
#include "IO/Observer/Actions/ObserverRegistration.hpp"
#include "IO/Observer/ObservationId.hpp"
#include "IO/Observer/ObserverComponent.hpp"
#include "IO/Observer/TypeOfObservation.hpp"
#include "Parallel/AlgorithmExecution.hpp"
#include "Parallel/ArrayIndex.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/Local.hpp"
#include "Parallel/Protocols/ElementRegistrar.hpp"
#include "Utilities/ProtocolHelpers.hpp"

/// \cond
template <class... Tags>
class TaggedTuple;
/// \endcond

namespace observers::Actions {
/*!
 * \brief Register an observation ID with the observers.
 *
 * \warning If registering events, you should use RegisterEventsWithObservers
 * instead. If your event is not compatible with RegisterEventsWithObservers,
 * please make it so.
 *
 * The `RegisterHelper` passed as a template parameter must have a static
 * `register_info` function that takes as its first template parameter the
 * `ParallelComponent` and as function arguments a `db::DataBox` and the array
 * component index. The function must return a
 * `std::pair<observers::TypeOfObservation, observers::ObservationId>`
 *
 * When this struct is used as an action, the `apply` function will perform the
 * registration with observers. However, this struct also offers the static
 * member functions `perform_registriation` and `perform_deregistration` that
 * are needed for either registering when an element is added to a core outside
 * of initialization or deregistering when an element is being eliminated from a
 * core. The use of separate functions is necessary to provide an interface
 * usable outside of iterable actions, e.g. in specialized `pup` functions.
 */
template <typename RegisterHelper>
struct RegisterWithObservers
    : tt::ConformsTo<Parallel::protocols::ElementRegistrar> {
 private:
  template <typename ParallelComponent, typename RegisterOrDeregisterAction,
            typename DbTagList, typename Metavariables, typename ArrayIndex>
  static void register_or_deregister_impl(
      db::DataBox<DbTagList>& box, Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& array_index) {
    auto& observer = *Parallel::local_branch(
        Parallel::get_parallel_component<observers::Observer<Metavariables>>(
            cache));
    const auto [type_of_observation, observation_id] =
        RegisterHelper::template register_info<ParallelComponent>(box,
                                                                  array_index);

    Parallel::simple_action<RegisterOrDeregisterAction>(
        observer, observation_id,
        observers::ArrayComponentId(
            std::add_pointer_t<ParallelComponent>{nullptr},
            Parallel::ArrayIndex<std::decay_t<ArrayIndex>>{array_index}),
        type_of_observation);
  }

 public:  // ElementRegistrar protocol
  template <typename ParallelComponent, typename DbTagList,
            typename Metavariables, typename ArrayIndex>
  static void perform_registration(db::DataBox<DbTagList>& box,
                                   Parallel::GlobalCache<Metavariables>& cache,
                                   const ArrayIndex& array_index) {
    register_or_deregister_impl<ParallelComponent,
                                RegisterContributorWithObserver>(box, cache,
                                                                 array_index);
  }

  template <typename ParallelComponent, typename DbTagList,
            typename Metavariables, typename ArrayIndex>
  static void perform_deregistration(
      db::DataBox<DbTagList>& box, Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& array_index) {
    register_or_deregister_impl<ParallelComponent,
                                DeregisterContributorWithObserver>(box, cache,
                                                                   array_index);
  }

 public:  // Iterable action
  template <typename DbTagList, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTagList>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& array_index, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    perform_registration<ParallelComponent>(box, cache, array_index);
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};
}  // namespace observers::Actions
