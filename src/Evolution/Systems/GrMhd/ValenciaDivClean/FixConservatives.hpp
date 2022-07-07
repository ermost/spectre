// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <limits>

#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/TagsDeclarations.hpp"  // IWYU pragma: keep
#include "Options/Options.hpp"
#include "PointwiseFunctions/GeneralRelativity/TagsDeclarations.hpp"  // IWYU pragma: keep
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace PUP {
class er;
}  // namespace PUP
class DataVector;
/// \endcond

// IWYU pragma: no_forward_declare EquationsOfState::EquationOfState
// IWYU pragma: no_forward_declare Tensor
// IWYU pragma: no_forward_declare grmhd::ValenciaDivClean::Tags::TildeD
// IWYU pragma: no_forward_declare grmhd::ValenciaDivClean::Tags::TildeYe
// IWYU pragma: no_forward_declare grmhd::ValenciaDivClean::Tags::TildeTau
// IWYU pragma: no_forward_declare grmhd::ValenciaDivClean::Tags::TildeS
// IWYU pragma: no_forward_declare grmhd::ValenciaDivClean::Tags::TildeB
// IWYU pragma: no_forward_declare gr::Tags::SpatialMetric
// IWYU pragma: no_forward_declare gr::Tags::InverseSpatialMetric
// IWYU pragma: no_forward_declare gr::Tags::SqrtDetSpatialMetric

namespace grmhd {
namespace ValenciaDivClean {

/*!
 * \ingroup VariableFixingGroup
 * \brief Fix conservative variables using method developed by Foucart.
 *
 * Adjusts the conservative variables as follows:
 * - Changes \f${\tilde D}\f$, the generalized mass-energy density, such
 *   that \f$D\f$, the product of the rest mass density \f$\rho\f$ and the
 *   Lorentz factor \f$W\f$, is set to value of the option `MinimumValueOfD`,
 *   whenever \f$D\f$ is below the value of the option `CutoffD`.
 * - Increases \f${\tilde \tau}\f$, the generalized internal energy density,
 *   such that
 *   \f${\tilde B}^2 \leq 2 \sqrt{\gamma} (1 - \epsilon_B) {\tilde \tau}\f$,
 *   where \f${\tilde B}^i\f$ is the generalized magnetic field,
 *   \f$\gamma\f$ is the determinant of the spatial metric, and
 *   \f$\epsilon_B\f$ is the option `SafetyFactorForB`.
 * - Decreases \f${\tilde S}_i\f$, the generalized momentum density, such that
 *   \f${\tilde S}^2 \leq (1 - \epsilon_S) {\tilde S}^2_{max}\f$, where
 *   \f$\epsilon_S\f$ is the option `SafetyFactorForS`, and
 *   \f${\tilde S}^2_{max}\f$ is a complicated function of the conservative
 *   variables which can only be found through root finding. There are
 *   sufficient conditions for a set of conservative variables to satisfy the
 *   inequality, which can be used to avoid root finding at most points.
 *
 * \note The routine currently assumes the minimum specific enthalpy is one.
 *
 * For more details see Appendix B from the [thesis of Francois
 * Foucart](https://ecommons.cornell.edu/handle/1813/30652)
 *
 * You can plot the function whose root we are finding using:
 *
 * \code{.py}
 *
 * import numpy as np
 * import matplotlib.pyplot as plt
 *
 * upper_bound = 1.000119047987896748e+00
 * lower_bound = 1.000000000000000000e+00
 * s_tilde_squared = 5.513009056734747750e-30
 * d_tilde = 1.131468709980503465e-12
 * sqrt_det_g: 1.131468709980503418e+00
 * tau_tilde = 1.346990732914080573e-16
 * b_tilde_squared = 3.048155733848927391e-16
 * b_squared_over_d = 2.380959757934347320e-04
 * tau_over_d = 1.190479878968363843e-04
 * normalized_s_dot_b = -9.999999082462245337e-01
 *
 *
 * def function_of_w(lorentz_factor):
 *     return ((lorentz_factor + b_squared_over_d - tau_over_d - 1.0) *
 *             (lorentz_factor**2 + b_squared_over_d * normalized_s_dot_b**2 *
 *              (b_squared_over_d + 2.0 * lorentz_factor)) -
 *             0.5 * b_squared_over_d -
 *             0.5 * b_squared_over_d * normalized_s_dot_b**2 *
 *             (lorentz_factor**2 - 1.0 +
 *              2.0 * lorentz_factor * b_squared_over_d + b_squared_over_d**2))
 *
 *
 * lorentz_factor = np.linspace(lower_bound, upper_bound, num=10000)
 *
 * plt.plot(lorentz_factor, function_of_w(lorentz_factor))
 * plt.show()
 *
 * \endcode
 */
class FixConservatives {
 public:
  /// \brief Minimum value of rest-mass density times lorentz factor
  struct MinimumValueOfD {
    using type = double;
    static type lower_bound() { return 0.0; }
    static constexpr Options::String help = {
        "Minimum value of rest-mass density times lorentz factor"};
  };
  /// \brief Cutoff below which \f$D = \rho W\f$ is set to MinimumValueOfD
  struct CutoffD {
    using type = double;
    static type lower_bound() { return 0.0; }
    static constexpr Options::String help = {
        "Cutoff below which D is set to MinimumValueOfD"};
  };

  /// \brief Safety factor \f$\epsilon_B\f$.
  struct SafetyFactorForB {
    using type = double;
    static type lower_bound() { return 0.0; }
    static constexpr Options::String help = {
        "Safety factor for magnetic field bound."};
  };
  /// \brief Safety factor \f$\epsilon_S\f$.
  struct SafetyFactorForS {
    using type = double;
    static type lower_bound() { return 0.0; }
    static constexpr Options::String help = {
        "Safety factor for momentum density bound."};
  };
  using options =
      tmpl::list<MinimumValueOfD, CutoffD, SafetyFactorForB, SafetyFactorForS>;
  static constexpr Options::String help = {
      "Variable fixing used in Foucart's thesis.\n"};

  FixConservatives(double minimum_rest_mass_density_times_lorentz_factor,
                   double rest_mass_density_times_lorentz_factor_cutoff,
                   double safety_factor_for_magnetic_field,
                   double safety_factor_for_momentum_density,
                   const Options::Context& context = {});

  FixConservatives() = default;
  FixConservatives(const FixConservatives& /*rhs*/) = default;
  FixConservatives& operator=(const FixConservatives& /*rhs*/) = default;
  FixConservatives(FixConservatives&& /*rhs*/) = default;
  FixConservatives& operator=(FixConservatives&& /*rhs*/) = default;
  ~FixConservatives() = default;

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& p);

  using return_tags = tmpl::list<grmhd::ValenciaDivClean::Tags::TildeD,
                                 grmhd::ValenciaDivClean::Tags::TildeYe,
                                 grmhd::ValenciaDivClean::Tags::TildeTau,
                                 grmhd::ValenciaDivClean::Tags::TildeS<>>;
  using argument_tags =
      tmpl::list<grmhd::ValenciaDivClean::Tags::TildeB<>,
                 gr::Tags::SpatialMetric<3>, gr::Tags::InverseSpatialMetric<3>,
                 gr::Tags::SqrtDetSpatialMetric<>>;

  /// Returns `true` if any variables were fixed.
  bool operator()(
      gsl::not_null<Scalar<DataVector>*> tilde_d,
      gsl::not_null<Scalar<DataVector>*> tilde_ye,
      gsl::not_null<Scalar<DataVector>*> tilde_tau,
      gsl::not_null<tnsr::i<DataVector, 3, Frame::Inertial>*> tilde_s,
      const tnsr::I<DataVector, 3, Frame::Inertial>& tilde_b,
      const tnsr::ii<DataVector, 3, Frame::Inertial>& spatial_metric,
      const tnsr::II<DataVector, 3, Frame::Inertial>& inv_spatial_metric,
      const Scalar<DataVector>& sqrt_det_spatial_metric) const;

 private:
  friend bool operator==(const FixConservatives& lhs,
                         const FixConservatives& rhs);

  double minimum_rest_mass_density_times_lorentz_factor_{
      std::numeric_limits<double>::signaling_NaN()};
  double rest_mass_density_times_lorentz_factor_cutoff_{
      std::numeric_limits<double>::signaling_NaN()};
  double one_minus_safety_factor_for_magnetic_field_{
      std::numeric_limits<double>::signaling_NaN()};
  double one_minus_safety_factor_for_momentum_density_{
      std::numeric_limits<double>::signaling_NaN()};
};

bool operator!=(const FixConservatives& lhs, const FixConservatives& rhs);

}  // namespace ValenciaDivClean
}  // namespace grmhd
