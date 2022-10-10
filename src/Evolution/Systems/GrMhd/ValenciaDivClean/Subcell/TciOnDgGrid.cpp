// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/Systems/GrMhd/ValenciaDivClean/Subcell/TciOnDgGrid.hpp"

#include <cstddef>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tags/TempTensor.hpp"
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Evolution/DgSubcell/PerssonTci.hpp"
#include "Evolution/DgSubcell/Projection.hpp"
#include "Evolution/DgSubcell/RdmpTci.hpp"
#include "Evolution/DgSubcell/RdmpTciData.hpp"
#include "Evolution/DgSubcell/SubcellOptions.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/KastaunEtAl.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/NewmanHamlin.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/PalenzuelaEtAl.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/PrimitiveFromConservative.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Subcell/TciOptions.hpp"
#include "NumericalAlgorithms/Spectral/Mesh.hpp"
#include "PointwiseFunctions/Hydro/EquationsOfState/EquationOfState.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"

namespace grmhd::ValenciaDivClean::subcell {
template <typename RecoveryScheme>
template <size_t ThermodynamicDim>
std::tuple<int, evolution::dg::subcell::RdmpTciData>
TciOnDgGrid<RecoveryScheme>::apply(
    const gsl::not_null<Variables<hydro::grmhd_tags<DataVector>>*> dg_prim_vars,
    const Scalar<DataVector>& tilde_d, const Scalar<DataVector>& tilde_ye,
    const Scalar<DataVector>& tilde_tau,
    const tnsr::i<DataVector, 3, Frame::Inertial>& tilde_s,
    const tnsr::I<DataVector, 3, Frame::Inertial>& tilde_b,
    const Scalar<DataVector>& tilde_phi,
    const tnsr::ii<DataVector, 3, Frame::Inertial>& spatial_metric,
    const tnsr::II<DataVector, 3, Frame::Inertial>& inv_spatial_metric,
    const Scalar<DataVector>& sqrt_det_spatial_metric,
    const EquationsOfState::EquationOfState<true, ThermodynamicDim>& eos,
    const Mesh<3>& dg_mesh, const Mesh<3>& subcell_mesh,
    const evolution::dg::subcell::RdmpTciData& past_rdmp_tci_data,
    const TciOptions& tci_options,
    const evolution::dg::subcell::SubcellOptions& subcell_options,
    const double persson_exponent) {
  evolution::dg::subcell::RdmpTciData rdmp_tci_data{};

  using std::max;
  using std::min;
  const size_t num_dg_pts = dg_mesh.number_of_grid_points();
  const size_t num_subcell_pts = subcell_mesh.number_of_grid_points();
  DataVector temp_buffer{4 * num_subcell_pts + num_dg_pts};
  size_t offset_into_temp_buffer = 0;
  const auto assign_data =
      [&temp_buffer, &offset_into_temp_buffer](
          const gsl::not_null<Scalar<DataVector>*> to_assign,
          const size_t size) {
        get(*to_assign)
            .set_data_ref(temp_buffer.data() + offset_into_temp_buffer, size);
        offset_into_temp_buffer += size;
      };
  Scalar<DataVector> subcell_tilde_d{};
  assign_data(make_not_null(&subcell_tilde_d), num_subcell_pts);
  evolution::dg::subcell::fd::project(make_not_null(&get(subcell_tilde_d)),
                                      get(tilde_d), dg_mesh,
                                      subcell_mesh.extents());

  Scalar<DataVector> subcell_tilde_ye{};
  assign_data(make_not_null(&subcell_tilde_ye), num_subcell_pts);
  evolution::dg::subcell::fd::project(make_not_null(&get(subcell_tilde_ye)),
                                      get(tilde_ye), dg_mesh,
                                      subcell_mesh.extents());

  Scalar<DataVector> subcell_tilde_tau{};
  assign_data(make_not_null(&subcell_tilde_tau), num_subcell_pts);
  evolution::dg::subcell::fd::project(make_not_null(&get(subcell_tilde_tau)),
                                      get(tilde_tau), dg_mesh,
                                      subcell_mesh.extents());

  Scalar<DataVector> mag_tilde_b{};
  assign_data(make_not_null(&mag_tilde_b), num_dg_pts);
  magnitude(make_not_null(&mag_tilde_b), tilde_b);
  Scalar<DataVector> subcell_mag_tilde_b{};
  assign_data(make_not_null(&subcell_mag_tilde_b), num_subcell_pts);
  evolution::dg::subcell::fd::project(make_not_null(&get(subcell_mag_tilde_b)),
                                      get(mag_tilde_b), dg_mesh,
                                      subcell_mesh.extents());
  const double max_mag_tilde_b = max(get(mag_tilde_b));

  rdmp_tci_data.max_variables_values =
      std::vector{max(max(get(subcell_tilde_d)), max(get(tilde_d))),
                  max(max(get(subcell_tilde_ye)), max(get(tilde_ye))),
                  max(max(get(subcell_tilde_tau)), max(get(tilde_tau))),
                  max(max(get(subcell_mag_tilde_b)), max_mag_tilde_b)};
  rdmp_tci_data.min_variables_values =
      std::vector{min(min(get(subcell_tilde_d)), min(get(tilde_d))),
                  min(min(get(subcell_tilde_ye)), min(get(tilde_ye))),
                  min(min(get(subcell_tilde_tau)), min(get(tilde_tau))),
                  min(min(get(subcell_mag_tilde_b)), min(get(mag_tilde_b)))};

  const double average_sqrt_det_spatial_metric =
      l1Norm(get(sqrt_det_spatial_metric));

  // require: tilde_d/avg(sqrt{gamma}) >= 0.0 (or some positive user-specified
  // value)
  if (min(get(tilde_d)) / average_sqrt_det_spatial_metric <
          tci_options.minimum_rest_mass_density_times_lorentz_factor or
      min(get(subcell_tilde_d)) / average_sqrt_det_spatial_metric <
          tci_options.minimum_rest_mass_density_times_lorentz_factor or
      min(get(tilde_ye)) / average_sqrt_det_spatial_metric <
          tci_options.minimum_rest_mass_density_times_lorentz_factor *
              tci_options.minimum_ye or
      min(get(subcell_tilde_ye)) / average_sqrt_det_spatial_metric <
          tci_options.minimum_rest_mass_density_times_lorentz_factor *
              tci_options.minimum_ye) {
    return {-1, std::move(rdmp_tci_data)};
  }

  // require: tilde_tau >= 0.0 (or some positive user-specified value)
  if (min(get(tilde_tau)) < tci_options.minimum_tilde_tau or
      min(get(subcell_tilde_tau)) < tci_options.minimum_tilde_tau) {
    return {-2, std::move(rdmp_tci_data)};
  }

  // Check if we are in atmosphere (but not negative tildeD), and if so, then
  // we continue using DG on this element.
  if (max(get(tilde_d) /
          (get(sqrt_det_spatial_metric) *
           get(get<hydro::Tags::LorentzFactor<DataVector>>(*dg_prim_vars)))) <
          tci_options.atmosphere_density and
      max(get(get<hydro::Tags::RestMassDensity<DataVector>>(*dg_prim_vars))) <
          tci_options.atmosphere_density) {
    // In atmosphere, we only need to recover the primitive variables for the
    // magnetic field and divergence cleaning field
    for (size_t i = 0; i < 3; ++i) {
      get<hydro::Tags::MagneticField<DataVector, 3>>(*dg_prim_vars).get(i) =
          tilde_b.get(i) / get(sqrt_det_spatial_metric);
    }
    get(get<hydro::Tags::DivergenceCleaningField<DataVector>>(*dg_prim_vars)) =
        get(tilde_phi) / get(sqrt_det_spatial_metric);

    return {false, std::move(rdmp_tci_data)};
  }

  Variables<hydro::grmhd_tags<DataVector>> temp_prims(num_dg_pts);
  {
    // require: tilde{B}^2 <= 2sqrt{gamma}(1-epsilon_B)\tilde{tau}
    Scalar<DataVector>& tilde_b_squared =
        get<hydro::Tags::RestMassDensity<DataVector>>(temp_prims);
    dot_product(make_not_null(&tilde_b_squared), tilde_b, tilde_b,
                spatial_metric);
    for (size_t i = 0; i < num_dg_pts; ++i) {
      if (get(tilde_b_squared)[i] >
          (1.0 - tci_options.safety_factor_for_magnetic_field) * 2.0 *
              get(tilde_tau)[i] * get(sqrt_det_spatial_metric)[i]) {
        return {-3, std::move(rdmp_tci_data)};
      }
    }
  }

  // Try to recover the primitive variables.
  // We assign them to a temporary so that if recovery fails at any of the
  // points we can use the valid primitives at the current time to provide a
  // high-order initial guess for the recovery on the subcells.
  //
  // Copy over the pressure since it's used as an initial guess in some
  // recovery schemes.
  get<hydro::Tags::Pressure<DataVector>>(temp_prims) =
      get<hydro::Tags::Pressure<DataVector>>(*dg_prim_vars);

  if (not grmhd::ValenciaDivClean::
          PrimitiveFromConservative<tmpl::list<RecoveryScheme>, false>::apply(
              make_not_null(
                  &get<hydro::Tags::RestMassDensity<DataVector>>(temp_prims)),
              make_not_null(
                  &get<hydro::Tags::ElectronFraction<DataVector>>(temp_prims)),
              make_not_null(
                  &get<hydro::Tags::SpecificInternalEnergy<DataVector>>(
                      temp_prims)),
              make_not_null(&get<hydro::Tags::SpatialVelocity<DataVector, 3>>(
                  temp_prims)),
              make_not_null(
                  &get<hydro::Tags::MagneticField<DataVector, 3>>(temp_prims)),
              make_not_null(
                  &get<hydro::Tags::DivergenceCleaningField<DataVector>>(
                      temp_prims)),
              make_not_null(
                  &get<hydro::Tags::LorentzFactor<DataVector>>(temp_prims)),
              make_not_null(
                  &get<hydro::Tags::Pressure<DataVector>>(temp_prims)),
              make_not_null(
                  &get<hydro::Tags::SpecificEnthalpy<DataVector>>(temp_prims)),
              tilde_d, tilde_ye, tilde_tau, tilde_s, tilde_b, tilde_phi,
              spatial_metric, inv_spatial_metric, sqrt_det_spatial_metric,
              eos)) {
    return {-4, std::move(rdmp_tci_data)};
  }

  // Check if we are in atmosphere after recovery. Unlikely we'd hit this and
  // not the check before the recovery, but just in case.
  if (max(get(get<hydro::Tags::RestMassDensity<DataVector>>(temp_prims))) <
      tci_options.atmosphere_density) {
    *dg_prim_vars = std::move(temp_prims);
    return {false, std::move(rdmp_tci_data)};
  }

  // Check that tilde_d and tilde_tau satisfy the Persson TCI
  if (evolution::dg::subcell::persson_tci(tilde_d, dg_mesh, persson_exponent) or
      evolution::dg::subcell::persson_tci(tilde_ye, dg_mesh,
                                          persson_exponent) or
      evolution::dg::subcell::persson_tci(tilde_tau, dg_mesh,
                                          persson_exponent)) {
    return {-5, std::move(rdmp_tci_data)};
  }
  // Check Cartesian magnitude of magnetic field satisfies the Persson TCI
  if (tci_options.magnetic_field_cutoff.has_value() and
      max_mag_tilde_b > tci_options.magnetic_field_cutoff.value() and
      evolution::dg::subcell::persson_tci(mag_tilde_b, dg_mesh,
                                          persson_exponent)) {
    return {-6, std::move(rdmp_tci_data)};
  }

  if (const int rdmp_tci_status = evolution::dg::subcell::rdmp_tci(
          rdmp_tci_data.max_variables_values,
          rdmp_tci_data.min_variables_values,
          past_rdmp_tci_data.max_variables_values,
          past_rdmp_tci_data.min_variables_values,
          subcell_options.rdmp_delta0(), subcell_options.rdmp_epsilon())) {
    return {-(6 + rdmp_tci_status), std::move(rdmp_tci_data)};
  }

  *dg_prim_vars = std::move(temp_prims);
  return {false, std::move(rdmp_tci_data)};
}

#define RECOVERY(data) BOOST_PP_TUPLE_ELEM(0, data)
#define INSTANTIATION(r, data) template class TciOnDgGrid<RECOVERY(data)>;
GENERATE_INSTANTIATIONS(
    INSTANTIATION,
    (grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::KastaunEtAl,
     grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin,
     grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl))
#undef INSTANTIATION

#define THERMO_DIM(data) BOOST_PP_TUPLE_ELEM(1, data)
#define INSTANTIATION(r, data)                                               \
  template std::tuple<int, evolution::dg::subcell::RdmpTciData>             \
  TciOnDgGrid<RECOVERY(data)>::apply<THERMO_DIM(data)>(                      \
      const gsl::not_null<Variables<hydro::grmhd_tags<DataVector>>*>         \
          dg_prim_vars,                                                      \
      const Scalar<DataVector>& tilde_d, const Scalar<DataVector>& tilde_ye, \
      const Scalar<DataVector>& tilde_tau,                                   \
      const tnsr::i<DataVector, 3, Frame::Inertial>& tilde_s,                \
      const tnsr::I<DataVector, 3, Frame::Inertial>& tilde_b,                \
      const Scalar<DataVector>& tilde_phi,                                   \
      const tnsr::ii<DataVector, 3, Frame::Inertial>& spatial_metric,        \
      const tnsr::II<DataVector, 3, Frame::Inertial>& inv_spatial_metric,    \
      const Scalar<DataVector>& sqrt_det_spatial_metric,                     \
      const EquationsOfState::EquationOfState<true, THERMO_DIM(data)>& eos,  \
      const Mesh<3>& dg_mesh, const Mesh<3>& subcell_mesh,                   \
      const evolution::dg::subcell::RdmpTciData& past_rdmp_tci_data,         \
      const TciOptions& tci_options,                                         \
      const evolution::dg::subcell::SubcellOptions& subcell_options,         \
      const double persson_exponent);

GENERATE_INSTANTIATIONS(
    INSTANTIATION,
    (grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::KastaunEtAl,
     grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin,
     grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl),
    (1, 2))
#undef INSTANTIATION
#undef THERMO_DIM
#undef RECOVERY
}  // namespace grmhd::ValenciaDivClean::subcell
