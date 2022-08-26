// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"

namespace grmhd::GhValenciaDivClean {
/// %Tags for the combined system of the Generalized Harmonic formulation for
/// the Einstein field equations and the Valencia GRMHD formulation.
namespace Tags {
namespace detail {
// A const reference to another tag, used for rerouting arguments in the
// combined system utilities
template <typename Tag, typename Type = typename Tag::type>
struct TemporaryReference {
  using tag = Tag;
  using type = const Type&;
};
}  // namespace detail

/// Represents the trace reversed stress-energy tensor of the matter in the MHD
/// sector of the GRMHD system
struct TraceReversedStressEnergy : db::SimpleTag {
  using type = tnsr::aa<DataVector, 3>;
};

/// Represents the stress-energy tensor of the matter in the MHD sector of the
/// GRMHD system
struct StressEnergy : db::SimpleTag {
  using type = tnsr::aa<DataVector, 3>;
};

/// The comoving magnetic field \f$b^\mu\f$
struct ComovingMagneticField : db::SimpleTag {
  using type = tnsr::A<DataVector, 3>;
};

/// The fluid four-velocity \f$u^\mu\f$
struct FourVelocity : db::SimpleTag {
  using type = tnsr::A<DataVector, 3>;
};

/// The down-index comoving magnetic field \f$b_\mu\f$
struct ComovingMagneticFieldOneForm : db::SimpleTag {
  using type = tnsr::a<DataVector, 3>;
};

/// The down-index four-velocity \f$u_\mu\f$
struct FourVelocityOneForm : db::SimpleTag {
  using type = tnsr::a<DataVector, 3>;
};
}  // namespace Tags
}  // namespace grmhd::GhValenciaDivClean
