// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <string>
#include <tuple>
#include <utility>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/Tag.hpp"
#include "Framework/ActionTesting.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/ParallelComponentHelpers.hpp"
#include "Parallel/Phase.hpp"
#include "Parallel/PhaseDependentActionList.hpp"  // IWYU pragma: keep
#include "ParallelAlgorithms/Actions/RemoveOptionsAndTerminatePhase.hpp"
#include "ParallelAlgorithms/Initialization/MutateAssign.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

namespace {
struct TemporalId {};

struct InitialTime : db::SimpleTag {
  using type = double;
};

struct InitialMass : db::SimpleTag {
  using type = double;
};

struct DummyTime : db::SimpleTag {
  using type = double;
};

template <typename Tag>
struct MultiplyByTwo : db::SimpleTag {
  using type = double;
};

template <typename Tag>
struct MultiplyByTwoCompute : MultiplyByTwo<Tag>, db::ComputeTag {
  using return_type = double;
  using base = MultiplyByTwo<Tag>;
  static void function(const gsl::not_null<double*> result, const double t) {
    *result = t * 2.0;
  }
  using argument_tags = tmpl::list<Tag>;
};

struct Action0 {
  using initialization_tags = tmpl::list<InitialTime>;
  using simple_tags = tmpl::list<DummyTime>;

  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent,
            Requires<tmpl::list_contains_v<DbTagsList, InitialTime>> = nullptr>
  static auto apply(db::DataBox<DbTagsList>& box,
                    const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
                    const Parallel::GlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, ActionList /*meta*/,
                    const ParallelComponent* const /*meta*/) {
    const double initial_time_value = db::get<InitialTime>(box);
    ::Initialization::mutate_assign<simple_tags>(make_not_null(&box),
                                                 3.0 * initial_time_value);
    return std::make_tuple(std::move(box));
  }

  template <
      typename DbTagsList, typename... InboxTags, typename Metavariables,
      typename ArrayIndex, typename ActionList, typename ParallelComponent,
      Requires<not tmpl::list_contains_v<DbTagsList, InitialTime>> = nullptr>
  static std::tuple<db::DataBox<DbTagsList>&&> apply(
      db::DataBox<DbTagsList>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      const Parallel::GlobalCache<Metavariables>& /*cache*/,
      const ArrayIndex& /*array_index*/, ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    return {std::move(box)};
  }
};

struct Action1 {
  using initialization_tags = tmpl::list<InitialMass>;
  using initialization_tags_to_keep = tmpl::list<InitialMass>;
  using compute_tags = tmpl::list<MultiplyByTwoCompute<DummyTime>>;
  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static auto apply(db::DataBox<DbTagsList>& box,
                    const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
                    const Parallel::GlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, ActionList /*meta*/,
                    const ParallelComponent* const /*meta*/) {
    return std::make_tuple(std::move(box));
  }
};

template <typename Metavariables>
struct Component {
  using metavariables = Metavariables;
  using chare_type = ActionTesting::MockArrayChare;
  using array_index = int;

  // [actions]
  using initialization_actions =
      tmpl::list<Action0, Action1,
                 Initialization::Actions::RemoveOptionsAndTerminatePhase>;
  // [actions]

  using phase_dependent_action_list =
      tmpl::list<Parallel::PhaseActions<Parallel::Phase::Initialization,
                                        initialization_actions>>;
  using initialization_tags = Parallel::get_initialization_tags<
      Parallel::get_initialization_actions_list<phase_dependent_action_list>>;
};

struct Metavariables {
  using component_list = tmpl::list<Component<Metavariables>>;
};
}  // namespace

SPECTRE_TEST_CASE(
    "Unit.ParallelAlgorithms.Initialization.RemoveOptionsFromDataBox",
    "[Unit][ParallelAlgorithms]") {
  using MockRuntimeSystem = ActionTesting::MockRuntimeSystem<Metavariables>;
  using component = Component<Metavariables>;

  MockRuntimeSystem runner{{}};
  const double initial_time = 3.7;
  const double initial_mass = 2.1;
  ActionTesting::emplace_component<component>(&runner, 0, initial_time,
                                              initial_mass);
  ActionTesting::set_phase(make_not_null(&runner),
                           Parallel::Phase::Initialization);
  CHECK(ActionTesting::tag_is_retrievable<component, InitialTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, InitialMass>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, DummyTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, MultiplyByTwo<DummyTime>>(
      runner, 0));
  CHECK(ActionTesting::get_databox_tag<component, InitialTime>(runner, 0) ==
        initial_time);
  CHECK_FALSE(ActionTesting::get_terminate<component>(runner, 0));
  // Runs Action0
  runner.next_action<component>(0);
  CHECK(ActionTesting::tag_is_retrievable<component, InitialTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, InitialMass>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, DummyTime>(runner, 0));
  CHECK(ActionTesting::get_databox_tag<component, InitialTime>(runner, 0) ==
        initial_time);
  CHECK(ActionTesting::get_databox_tag<component, InitialMass>(runner, 0) ==
        initial_mass);
  CHECK(ActionTesting::get_databox_tag<component, DummyTime>(runner, 0) ==
        3.0 * initial_time);
  CHECK_FALSE(ActionTesting::get_terminate<component>(runner, 0));
  // Runs Action1
  runner.next_action<component>(0);
  CHECK(ActionTesting::tag_is_retrievable<component, InitialTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, InitialMass>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, DummyTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, MultiplyByTwo<DummyTime>>(
      runner, 0));
  CHECK(ActionTesting::get_databox_tag<component, InitialTime>(runner, 0) ==
        initial_time);
  CHECK(ActionTesting::get_databox_tag<component, InitialMass>(runner, 0) ==
        initial_mass);
  CHECK(ActionTesting::get_databox_tag<component, DummyTime>(runner, 0) ==
        3.0 * initial_time);
  CHECK(ActionTesting::get_databox_tag<component, MultiplyByTwo<DummyTime>>(
            runner, 0) == 6.0 * initial_time);
  CHECK_FALSE(ActionTesting::get_terminate<component>(runner, 0));
  // Runs RemoveOptionsFromDataBox
  runner.next_action<component>(0);
  CHECK(ActionTesting::tag_is_retrievable<component, InitialTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, InitialMass>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, DummyTime>(runner, 0));
  CHECK(ActionTesting::tag_is_retrievable<component, MultiplyByTwo<DummyTime>>(
      runner, 0));
#ifdef SPECTRE_DEBUG
  CHECK_THROWS_WITH(([&runner]() {
                      ActionTesting::get_databox_tag<component, InitialTime>(
                          runner, 0);
                    }()),
                    Catch::Contains("Unable to retrieve item 'InitialTime' as "
                                    "it has been removed from the DataBox."));
#endif
  CHECK(ActionTesting::get_databox_tag<component, InitialMass>(runner, 0) ==
        initial_mass);
  CHECK(ActionTesting::get_databox_tag<component, DummyTime>(runner, 0) ==
        3.0 * initial_time);
  CHECK(ActionTesting::get_databox_tag<component, MultiplyByTwo<DummyTime>>(
            runner, 0) == 6.0 * initial_time);
  CHECK(ActionTesting::get_terminate<component>(runner, 0));
}
