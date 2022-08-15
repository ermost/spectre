// Distributed under the MIT License.
// See LICENSE.txt for details.

#include <string>

#include "Evolution/Systems/GrMhd/ValenciaDivClean/Tags.hpp"
#include "Framework/TestingFramework.hpp"
#include "Helpers/DataStructures/DataBox/TestHelpers.hpp"

SPECTRE_TEST_CASE("Unit.Evolution.Systems.ValenciaDivClean.Tags",
                  "[Unit][Evolution]") {
  TestHelpers::db::test_simple_tag<
      grmhd::ValenciaDivClean::Tags::VariablesNeededFixing>(
      "VariablesNeededFixing");
}
