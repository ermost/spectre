// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include "Framework/TestingFramework.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/DataBoxTag.hpp"
#include "DataStructures/DynamicVector.hpp"
#include "Helpers/ParallelAlgorithms/LinearSolver/LinearSolverAlgorithmTestHelpers.hpp"
#include "IO/Observer/Helpers.hpp"
#include "IO/Observer/ObserverComponent.hpp"
#include "NumericalAlgorithms/Convergence/HasConverged.hpp"
#include "NumericalAlgorithms/Convergence/Tags.hpp"
#include "Options/String.hpp"
#include "Parallel/AlgorithmExecution.hpp"
#include "Parallel/Algorithms/AlgorithmArray.hpp"
#include "Parallel/Algorithms/AlgorithmSingleton.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/InitializationFunctions.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/Local.hpp"
#include "Parallel/Phase.hpp"
#include "ParallelAlgorithms/Actions/Goto.hpp"
#include "ParallelAlgorithms/Initialization/MutateAssign.hpp"
#include "ParallelAlgorithms/LinearSolver/Tags.hpp"
#include "ParallelAlgorithms/NonlinearSolver/Tags.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/ErrorHandling/FloatingPointExceptions.hpp"
#include "Utilities/FileSystem.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Requires.hpp"
#include "Utilities/TMPL.hpp"

namespace TestHelpers::NonlinearSolver {

struct Source : db::SimpleTag {
  static constexpr Options::String help = "The source b in the equation Ax=b.";
  using type = blaze::DynamicVector<double>;
  static constexpr bool pass_metavariables = false;
  using option_tags = tmpl::list<Source>;
  static type create_from_options(const type& option) { return option; }
};
struct InitialGuess : db::SimpleTag {
  static constexpr Options::String help = "The initial guess for the vector x.";
  using type = blaze::DynamicVector<double>;
  static constexpr bool pass_metavariables = false;
  using option_tags = tmpl::list<InitialGuess>;
  static type create_from_options(const type& option) { return option; }
};
struct ExpectedResult : db::SimpleTag {
  static constexpr Options::String help = "The solution x in the equation Ax=b";
  using type = blaze::DynamicVector<double>;
  static constexpr bool pass_metavariables = false;
  using option_tags = tmpl::list<ExpectedResult>;
  static type create_from_options(const type& option) { return option; }
};

// The vector `x` we want to solve for
struct VectorTag : db::SimpleTag {
  using type = blaze::DynamicVector<double>;
  static std::string name() { return "VectorTag"; }
};

using fields_tag = VectorTag;
using nonlinear_source_tag = ::Tags::FixedSource<fields_tag>;

// Checks for the correct solution after the algorithm has terminated.
template <typename OptionsGroup>
struct TestResult {
  using const_global_cache_tags = tmpl::list<ExpectedResult>;

  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            typename ActionList, typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTagsList>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      const Parallel::GlobalCache<Metavariables>& cache,
      const int /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    const auto& has_converged =
        get<Convergence::Tags::HasConverged<OptionsGroup>>(box);
    SPECTRE_PARALLEL_REQUIRE(has_converged);
    SPECTRE_PARALLEL_REQUIRE(has_converged.reason() ==
                             Convergence::Reason::AbsoluteResidual);
    const auto& result = get<VectorTag>(box);
    const auto& expected_result = get<ExpectedResult>(cache);
    for (size_t i = 0; i < expected_result.size(); i++) {
      SPECTRE_PARALLEL_REQUIRE(result[i] == approx(expected_result[i]));
    }
    return {Parallel::AlgorithmExecution::Pause, std::nullopt};
  }
};

struct InitializeElement {
  using const_global_cache_tags = tmpl::list<Source, InitialGuess>;
  using simple_tags = tmpl::list<fields_tag, nonlinear_source_tag>;

  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            typename ActionList, typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTagsList>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      const Parallel::GlobalCache<Metavariables>& cache,
      const int /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    const auto& b = get<Source>(cache);
    const auto& x0 = get<InitialGuess>(cache);

    ::Initialization::mutate_assign<simple_tags>(make_not_null(&box), x0, b);
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};

template <typename Metavariables>
struct ElementArray {
  using chare_type = Parallel::Algorithms::Array;
  using array_index = int;
  using metavariables = Metavariables;

  using nonlinear_solver = typename Metavariables::nonlinear_solver;
  using linear_solver = typename Metavariables::linear_solver;

  using phase_dependent_action_list = tmpl::list<
      Parallel::PhaseActions<
          Parallel::Phase::Initialization,
          tmpl::list<InitializeElement,
                     typename nonlinear_solver::initialize_element,
                     typename linear_solver::initialize_element,
                     Parallel::Actions::TerminatePhase>>,
      Parallel::PhaseActions<
          Parallel::Phase::Register,
          tmpl::list<typename nonlinear_solver::register_element,
                     typename linear_solver::register_element,
                     Parallel::Actions::TerminatePhase>>,
      Parallel::PhaseActions<
          Parallel::Phase::Solve,
          tmpl::list<
              typename nonlinear_solver::template solve<
                  typename Metavariables::template apply_nonlinear_operator<
                      typename nonlinear_solver::operand_tag>,
                  typename linear_solver::template solve<
                      typename Metavariables::
                          template apply_linearized_operator<
                              typename linear_solver::operand_tag,
                              typename nonlinear_solver::fields_tag>>>,
              Parallel::Actions::TerminatePhase>>,
      Parallel::PhaseActions<
          Parallel::Phase::Testing,
          tmpl::list<TestResult<typename nonlinear_solver::options_group>>>>;

  using simple_tags_from_options = Parallel::get_simple_tags_from_options<
      Parallel::get_initialization_actions_list<phase_dependent_action_list>>;

  static void allocate_array(
      Parallel::CProxy_GlobalCache<Metavariables>& global_cache,
      const tuples::tagged_tuple_from_typelist<simple_tags_from_options>&
          initialization_items,
      const std::unordered_set<size_t>& /*procs_to_ignore*/ = {}) {
    auto& local_component = Parallel::get_parallel_component<ElementArray>(
        *Parallel::local_branch(global_cache));
    local_component[0].insert(global_cache, initialization_items, 0);
    local_component.doneInserting();
  }

  static void execute_next_phase(
      const Parallel::Phase next_phase,
      Parallel::CProxy_GlobalCache<Metavariables>& global_cache) {
    auto& local_component = Parallel::get_parallel_component<ElementArray>(
        *Parallel::local_branch(global_cache));
    local_component.start_phase(next_phase);
  }
};

template <typename Metavariables>
using OutputCleaner =
    LinearSolverAlgorithmTestHelpers::OutputCleaner<Metavariables>;

static constexpr std::array<Parallel::Phase, 6> default_phase_order = {
    {Parallel::Phase::Initialization, Parallel::Phase::Register,
     Parallel::Phase::Solve, Parallel::Phase::Testing, Parallel::Phase::Cleanup,
     Parallel::Phase::Exit}};

template <typename Metavariables>
using component_list = tmpl::flatten<tmpl::list<
    typename Metavariables::nonlinear_solver::component_list,
    typename Metavariables::linear_solver::component_list,
    ElementArray<Metavariables>, observers::Observer<Metavariables>,
    observers::ObserverWriter<Metavariables>, OutputCleaner<Metavariables>>>;

template <typename Metavariables>
using observed_reduction_data_tags = observers::collect_reduction_data_tags<
    tmpl::list<typename Metavariables::linear_solver,
               typename Metavariables::nonlinear_solver>>;

}  // namespace TestHelpers::NonlinearSolver
