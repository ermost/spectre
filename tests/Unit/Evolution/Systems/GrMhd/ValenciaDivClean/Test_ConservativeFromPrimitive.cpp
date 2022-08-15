// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "DataStructures/DataVector.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/ConservativeFromPrimitive.hpp"
#include "Framework/CheckWithRandomValues.hpp"
#include "Framework/SetupLocalPythonEnvironment.hpp"
#include "Framework/TestingFramework.hpp"

// IWYU pragma: no_include "DataStructures/Tensor/Tensor.hpp"
// IWYU pragma: no_include "Utilities/Gsl.hpp"
// IWYU pragma: no_include <string>

SPECTRE_TEST_CASE("Unit.GrMhd.ValenciaDivClean.ConservativeFromPrimitive",
                  "[Unit][GrMhd]") {
  pypp::SetupLocalPythonEnvironment local_python_env{
      "Evolution/Systems/GrMhd/ValenciaDivClean"};

  pypp::check_with_random_values<1>(
      &grmhd::ValenciaDivClean::ConservativeFromPrimitive::apply,
      "TestFunctions",
      {"tilde_d", "tilde_ye", "tilde_tau", "tilde_s", "tilde_b", "tilde_phi"},
      {{{0.0, 1.0}}}, DataVector{6});
}
