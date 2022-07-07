// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/Systems/GrMhd/ValenciaDivClean/Subcell/InitialDataTci.hpp"

#include <cstddef>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Evolution/DgSubcell/ActiveGrid.hpp"
#include "Evolution/DgSubcell/PerssonTci.hpp"
#include "Evolution/DgSubcell/Projection.hpp"
#include "Evolution/DgSubcell/TwoMeshRdmpTci.hpp"
#include "NumericalAlgorithms/Spectral/Mesh.hpp"

namespace grmhd::ValenciaDivClean::subcell {
namespace detail {
std::tuple<bool, evolution::dg::subcell::RdmpTciData> initial_data_tci_work(
    const Scalar<DataVector>& dg_tilde_d, const Scalar<DataVector>& dg_tilde_ye,
    const Scalar<DataVector>& dg_tilde_tau,
    const Scalar<DataVector>& dg_tilde_b_magnitude,
    const Scalar<DataVector>& subcell_tilde_d,
    const Scalar<DataVector>& subcell_tilde_ye,
    const Scalar<DataVector>& subcell_tilde_tau,
    const Scalar<DataVector>& subcell_tilde_b_magnitude,
    const double persson_exponent, const Mesh<3>& dg_mesh,
    const TciOptions& tci_options) {
  evolution::dg::subcell::RdmpTciData rdmp_tci_data{};
  using std::max;
  using std::min;
  rdmp_tci_data.max_variables_values = std::vector<double>{
      max(max(get(dg_tilde_d)), max(get(subcell_tilde_d))),
      max(max(get(dg_tilde_ye)), max(get(subcell_tilde_ye))),
      max(max(get(dg_tilde_tau)), max(get(subcell_tilde_tau))),
      max(max(get(dg_tilde_b_magnitude)), max(get(subcell_tilde_b_magnitude)))};
  rdmp_tci_data.min_variables_values = std::vector<double>{
      min(min(get(dg_tilde_d)), min(get(subcell_tilde_d))),
      min(min(get(dg_tilde_ye)), min(get(subcell_tilde_ye))),
      min(min(get(dg_tilde_tau)), min(get(subcell_tilde_tau))),
      min(min(get(dg_tilde_b_magnitude)), min(get(subcell_tilde_b_magnitude)))};

  return {min(get(dg_tilde_d)) <
                  tci_options.minimum_rest_mass_density_times_lorentz_factor or
              min(get(subcell_tilde_d)) <
                  tci_options.minimum_rest_mass_density_times_lorentz_factor or
              min(get(dg_tilde_ye)) < tci_options.minimum_ye or
              min(get(subcell_tilde_ye)) < tci_options.minimum_ye or
              min(get(dg_tilde_tau)) < tci_options.minimum_tilde_tau or
              min(get(subcell_tilde_tau)) < tci_options.minimum_tilde_tau or
              evolution::dg::subcell::persson_tci(dg_tilde_d, dg_mesh,
                                                  persson_exponent) or
              evolution::dg::subcell::persson_tci(dg_tilde_ye, dg_mesh,
                                                  persson_exponent) or
              evolution::dg::subcell::persson_tci(dg_tilde_tau, dg_mesh,
                                                  persson_exponent) or
              (tci_options.magnetic_field_cutoff.has_value() and
               max(get(dg_tilde_b_magnitude)) >
                   tci_options.magnetic_field_cutoff.value() and
               evolution::dg::subcell::persson_tci(dg_tilde_b_magnitude,
                                                   dg_mesh, persson_exponent)),
          std::move(rdmp_tci_data)};
}
}  // namespace detail

std::tuple<bool, evolution::dg::subcell::RdmpTciData> DgInitialDataTci::apply(
    const Variables<tmpl::list<
        ValenciaDivClean::Tags::TildeD, ValenciaDivClean::Tags::TildeYe,
        ValenciaDivClean::Tags::TildeTau, ValenciaDivClean::Tags::TildeS<>,
        ValenciaDivClean::Tags::TildeB<>, ValenciaDivClean::Tags::TildePhi>>&
        dg_vars,
    const double rdmp_delta0, const double rdmp_epsilon,
    const double persson_exponent, const Mesh<3>& dg_mesh,
    const Mesh<3>& subcell_mesh, const TciOptions& tci_options) {
  const Scalar<DataVector> dg_tilde_b_magnitude =
      magnitude(get<ValenciaDivClean::Tags::TildeB<>>(dg_vars));
  const auto subcell_vars = evolution::dg::subcell::fd::project(
      dg_vars, dg_mesh, subcell_mesh.extents());
  const Scalar<DataVector> subcell_tilde_b_magnitude =
      magnitude(get<ValenciaDivClean::Tags::TildeB<>>(subcell_vars));

  auto result = detail::initial_data_tci_work(
      get<ValenciaDivClean::Tags::TildeD>(dg_vars),
      get<ValenciaDivClean::Tags::TildeYe>(dg_vars),
      get<ValenciaDivClean::Tags::TildeTau>(dg_vars), dg_tilde_b_magnitude,
      get<ValenciaDivClean::Tags::TildeD>(subcell_vars),
      get<ValenciaDivClean::Tags::TildeYe>(subcell_vars),
      get<ValenciaDivClean::Tags::TildeTau>(subcell_vars),
      subcell_tilde_b_magnitude, persson_exponent, dg_mesh, tci_options);
  return {std::get<0>(result) or
              evolution::dg::subcell::two_mesh_rdmp_tci(
                  dg_vars, subcell_vars, rdmp_delta0, rdmp_epsilon),
          std::move(std::get<1>(result))};
}

void SetInitialRdmpData::apply(
    const gsl::not_null<evolution::dg::subcell::RdmpTciData*> rdmp_tci_data,
    const Scalar<DataVector>& subcell_tilde_d,
    const Scalar<DataVector>& subcell_tilde_ye,
    const Scalar<DataVector>& subcell_tilde_tau,
    const tnsr::I<DataVector, 3, Frame::Inertial>& subcell_tilde_b,
    const evolution::dg::subcell::ActiveGrid active_grid) {
  if (active_grid == evolution::dg::subcell::ActiveGrid::Subcell) {
    const Scalar<DataVector> subcell_tilde_b_magnitude =
        magnitude(subcell_tilde_b);

    rdmp_tci_data->max_variables_values = std::vector<double>{
        max(get(subcell_tilde_d)), max(get(subcell_tilde_ye)),
        max(get(subcell_tilde_tau)), max(get(subcell_tilde_b_magnitude))};
    rdmp_tci_data->min_variables_values = std::vector<double>{
        min(get(subcell_tilde_d)), min(get(subcell_tilde_ye)),
        min(get(subcell_tilde_tau)), min(get(subcell_tilde_b_magnitude))};
  }
}
}  // namespace grmhd::ValenciaDivClean::subcell
