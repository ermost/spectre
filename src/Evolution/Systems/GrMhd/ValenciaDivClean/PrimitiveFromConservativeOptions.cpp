// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/Systems/GrMhd/ValenciaDivClean/PrimitiveFromConservativeOptions.hpp"

#include <pup.h>
#include <pup_stl.h>

namespace grmhd::ValenciaDivClean {

PrimitiveFromConservativeOptions::PrimitiveFromConservativeOptions(
    const double cutoff_d_for_inversion,
    const double density_when_skipping_inversion)
    : cutoff_d_for_inversion_(cutoff_d_for_inversion),
      density_when_skipping_inversion_(density_when_skipping_inversion) {}

void PrimitiveFromConservativeOptions::pup(PUP::er& p) {
  p | cutoff_d_for_inversion_;
  p | density_when_skipping_inversion_;
}

bool operator==(const PrimitiveFromConservativeOptions& lhs,
                const PrimitiveFromConservativeOptions& rhs) {
  return (lhs.cutoff_d_for_inversion_ == rhs.cutoff_d_for_inversion_) and
         (lhs.density_when_skipping_inversion_ ==
          rhs.density_when_skipping_inversion_);
}

bool operator!=(const PrimitiveFromConservativeOptions& lhs,
                const PrimitiveFromConservativeOptions& rhs) {
  return not(lhs == rhs);
}

}  // namespace grmhd::ValenciaDivClean
