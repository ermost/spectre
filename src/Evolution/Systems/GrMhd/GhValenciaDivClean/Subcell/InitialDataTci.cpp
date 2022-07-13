// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/Systems/GrMhd/GhValenciaDivClean/Subcell/InitialDataTci.hpp"

#include <cstddef>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Evolution/DgSubcell/TwoMeshRdmpTci.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Subcell/InitialDataTci.hpp"
#include "NumericalAlgorithms/Spectral/Mesh.hpp"

namespace grmhd::GhValenciaDivClean::subcell {
bool DgInitialDataTci::apply(
    const Variables<tmpl::list<
        gr::Tags::SpacetimeMetric<3, Frame::Inertial, DataVector>,
        GeneralizedHarmonic::Tags::Pi<3, Frame::Inertial>,
        GeneralizedHarmonic::Tags::Phi<3, Frame::Inertial>,
        ValenciaDivClean::Tags::TildeD, ValenciaDivClean::Tags::TildeYe,
        ValenciaDivClean::Tags::TildeTau, ValenciaDivClean::Tags::TildeS<>,
        ValenciaDivClean::Tags::TildeB<>, ValenciaDivClean::Tags::TildePhi>>&
        dg_vars,
    const Variables<tmpl::list<
        Inactive<gr::Tags::SpacetimeMetric<3, Frame::Inertial, DataVector>>,
        Inactive<GeneralizedHarmonic::Tags::Pi<3, Frame::Inertial>>,
        Inactive<GeneralizedHarmonic::Tags::Phi<3, Frame::Inertial>>,
        Inactive<ValenciaDivClean::Tags::TildeD>,
        Inactive<ValenciaDivClean::Tags::TildeYe>,
        Inactive<ValenciaDivClean::Tags::TildeTau>,
        Inactive<ValenciaDivClean::Tags::TildeS<>>,
        Inactive<ValenciaDivClean::Tags::TildeB<>>,
        Inactive<ValenciaDivClean::Tags::TildePhi>>>& subcell_vars,
    const double rdmp_delta0, const double rdmp_epsilon,
    const double persson_exponent, const Mesh<3>& dg_mesh,
    const ValenciaDivClean::subcell::TciOptions& tci_options) {
  const Scalar<DataVector> dg_tilde_b_magnitude =
      magnitude(get<ValenciaDivClean::Tags::TildeB<>>(dg_vars));
  const auto subcell_vars = evolution::dg::subcell::fd::project(
      dg_vars, dg_mesh, subcell_mesh.extents());
  const Scalar<DataVector> subcell_tilde_b_magnitude =
      magnitude(get<ValenciaDivClean::Tags::TildeB<>>(subcell_vars));
  return ValenciaDivClean::subcell::detail::initial_data_tci_work(
             get<ValenciaDivClean::Tags::TildeD>(dg_vars),
             get<ValenciaDivClean::Tags::TildeYe>(dg_vars),
             get<ValenciaDivClean::Tags::TildeTau>(dg_vars),
             dg_tilde_b_magnitude,
             get<Inactive<ValenciaDivClean::Tags::TildeD>>(subcell_vars),
             get<Inactive<ValenciaDivClean::Tags::TildeYe>>(subcell_vars),
             get<Inactive<ValenciaDivClean::Tags::TildeTau>>(subcell_vars),
             get<ValenciaDivClean::Tags::TildeB<>>(dg_vars), persson_exponent,
             dg_mesh, tci_options) or
         evolution::dg::subcell::two_mesh_rdmp_tci(dg_vars, subcell_vars,
                                                   rdmp_delta0, rdmp_epsilon);
}
}  // namespace grmhd::GhValenciaDivClean::subcell
