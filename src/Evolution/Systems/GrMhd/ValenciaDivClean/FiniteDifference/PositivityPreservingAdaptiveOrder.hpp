// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "DataStructures/FixedHashMap.hpp"
#include "DataStructures/VariablesTag.hpp"
#include "Domain/Structure/MaxNumberOfNeighbors.hpp"
#include "Domain/Tags.hpp"
#include "Evolution/DgSubcell/Tags/Mesh.hpp"
#include "Evolution/DgSubcell/Tags/NeighborData.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/FiniteDifference/Reconstructor.hpp"
#include "NumericalAlgorithms/FiniteDifference/FallbackReconstructorType.hpp"
#include "Options/Options.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
class DataVector;
template <size_t Dim>
class Direction;
template <size_t Dim>
class Element;
template <size_t Dim>
class ElementId;
namespace EquationsOfState {
template <bool IsRelativistic, size_t ThermodynamicDim>
class EquationOfState;
}  // namespace EquationsOfState
template <size_t Dim>
class Mesh;
namespace gsl {
template <typename T>
class not_null;
}  // namespace gsl
namespace PUP {
class er;
}  // namespace PUP
template <typename TagsList>
class Variables;
/// \endcond

namespace grmhd::ValenciaDivClean::fd {
/*!
 * \brief Positivity-preserving adaptive order reconstruction. See
 * ::fd::reconstruction::positivity_preserving_adaptive_order() for details.
 *
 * The rest mass density and the pressure are kept positive.
 */
class PositivityPreservingAdaptiveOrderPrim : public Reconstructor {
 private:
  using prims_to_reconstruct_tags =
      tmpl::list<hydro::Tags::RestMassDensity<DataVector>,
                 hydro::Tags::ElectronFraction<DataVector>,
                 hydro::Tags::Pressure<DataVector>,
                 hydro::Tags::LorentzFactorTimesSpatialVelocity<DataVector, 3>,
                 hydro::Tags::MagneticField<DataVector, 3>,
                 hydro::Tags::DivergenceCleaningField<DataVector>>;

  using positivity_preserving_tags =
      tmpl::list<hydro::Tags::RestMassDensity<DataVector>,
                 hydro::Tags::ElectronFraction<DataVector>>,
                 hydro::Tags::Pressure<DataVector>>;
  using non_positive_tags =
      tmpl::list<hydro::Tags::LorentzFactorTimesSpatialVelocity<DataVector, 3>,
                 hydro::Tags::MagneticField<DataVector, 3>,
                 hydro::Tags::DivergenceCleaningField<DataVector>>;

  using FallbackReconstructorType =
      ::fd::reconstruction::FallbackReconstructorType;

 public:
  static constexpr size_t dim = 3;

  struct Alpha5 {
    using type = double;
    static constexpr Options::String help = {
        "The alpha parameter in the Persson convergence measurement. 4 is the "
        "right value, but anything in the range of 3-5 is 'reasonable'. "
        "Smaller values allow for more oscillations."};
  };
  struct LowOrderReconstructor {
    using type = FallbackReconstructorType;
    static constexpr Options::String help = {
        "The 2nd/3rd-order reconstruction scheme to use if unlimited 5th-order "
        "isn't okay."};
  };

  using options = tmpl::list<Alpha5, LowOrderReconstructor>;

  static constexpr Options::String help{
      "Positivity-preserving adaptive-order reconstruction."};

  PositivityPreservingAdaptiveOrderPrim() = default;
  PositivityPreservingAdaptiveOrderPrim(
      PositivityPreservingAdaptiveOrderPrim&&) = default;
  PositivityPreservingAdaptiveOrderPrim& operator=(
      PositivityPreservingAdaptiveOrderPrim&&) = default;
  PositivityPreservingAdaptiveOrderPrim(
      const PositivityPreservingAdaptiveOrderPrim&) = default;
  PositivityPreservingAdaptiveOrderPrim& operator=(
      const PositivityPreservingAdaptiveOrderPrim&) = default;
  ~PositivityPreservingAdaptiveOrderPrim() override = default;

  PositivityPreservingAdaptiveOrderPrim(
      double alpha_5, FallbackReconstructorType low_order_reconstructor,
      const Options::Context& context = {});

  explicit PositivityPreservingAdaptiveOrderPrim(CkMigrateMessage* msg);

  WRAPPED_PUPable_decl_base_template(Reconstructor,
                                     PositivityPreservingAdaptiveOrderPrim);

  auto get_clone() const -> std::unique_ptr<Reconstructor> override;

  void pup(PUP::er& p) override;

  size_t ghost_zone_size() const override { return 3; }

  using reconstruction_argument_tags = tmpl::list<
      ::Tags::Variables<hydro::grmhd_tags<DataVector>>,
      hydro::Tags::EquationOfStateBase, domain::Tags::Element<dim>,
      evolution::dg::subcell::Tags::NeighborDataForReconstruction<dim>,
      evolution::dg::subcell::Tags::Mesh<dim>>;

  template <size_t ThermodynamicDim, typename TagsList>
  void reconstruct(
      gsl::not_null<std::array<Variables<TagsList>, dim>*> vars_on_lower_face,
      gsl::not_null<std::array<Variables<TagsList>, dim>*> vars_on_upper_face,
      const Variables<hydro::grmhd_tags<DataVector>>& volume_prims,
      const EquationsOfState::EquationOfState<true, ThermodynamicDim>& eos,
      const Element<dim>& element,
      const FixedHashMap<
          maximum_number_of_neighbors(dim),
          std::pair<Direction<dim>, ElementId<dim>>, std::vector<double>,
          boost::hash<std::pair<Direction<dim>, ElementId<dim>>>>&
          neighbor_data,
      const Mesh<dim>& subcell_mesh) const;

  template <size_t ThermodynamicDim, typename TagsList>
  void reconstruct_fd_neighbor(
      gsl::not_null<Variables<TagsList>*> vars_on_face,
      const Variables<hydro::grmhd_tags<DataVector>>& subcell_volume_prims,
      const EquationsOfState::EquationOfState<true, ThermodynamicDim>& eos,
      const Element<dim>& element,
      const FixedHashMap<
          maximum_number_of_neighbors(dim),
          std::pair<Direction<dim>, ElementId<dim>>, std::vector<double>,
          boost::hash<std::pair<Direction<dim>, ElementId<dim>>>>&
          neighbor_data,
      const Mesh<dim>& subcell_mesh,
      const Direction<dim> direction_to_reconstruct) const;

 private:
  // NOLINTNEXTLINE(readability-redundant-declaration)
  friend bool operator==(const PositivityPreservingAdaptiveOrderPrim& lhs,
                         const PositivityPreservingAdaptiveOrderPrim& rhs);
  friend bool operator!=(const PositivityPreservingAdaptiveOrderPrim& lhs,
                         const PositivityPreservingAdaptiveOrderPrim& rhs);

  double four_to_the_alpha_5_ = std::numeric_limits<double>::signaling_NaN();
  FallbackReconstructorType low_order_reconstructor_ =
      FallbackReconstructorType::None;

  void (*reconstruct_)(gsl::not_null<std::array<gsl::span<double>, dim>*>,
                       gsl::not_null<std::array<gsl::span<double>, dim>*>,
                       const gsl::span<const double>&,
                       const DirectionMap<dim, gsl::span<const double>>&,
                       const Index<dim>&, size_t, double) = nullptr;
  void (*reconstruct_lower_neighbor_)(gsl::not_null<DataVector*>,
                                      const DataVector&, const DataVector&,
                                      const Index<dim>&, const Index<dim>&,
                                      const Direction<dim>&,
                                      const double&) = nullptr;
  void (*reconstruct_upper_neighbor_)(gsl::not_null<DataVector*>,
                                      const DataVector&, const DataVector&,
                                      const Index<dim>&, const Index<dim>&,
                                      const Direction<dim>&,
                                      const double&) = nullptr;
  void (*pp_reconstruct_)(gsl::not_null<std::array<gsl::span<double>, dim>*>,
                          gsl::not_null<std::array<gsl::span<double>, dim>*>,
                          const gsl::span<const double>&,
                          const DirectionMap<dim, gsl::span<const double>>&,
                          const Index<dim>&, size_t, double) = nullptr;
  void (*pp_reconstruct_lower_neighbor_)(gsl::not_null<DataVector*>,
                                         const DataVector&, const DataVector&,
                                         const Index<dim>&, const Index<dim>&,
                                         const Direction<dim>&,
                                         const double&) = nullptr;
  void (*pp_reconstruct_upper_neighbor_)(gsl::not_null<DataVector*>,
                                         const DataVector&, const DataVector&,
                                         const Index<dim>&, const Index<dim>&,
                                         const Direction<dim>&,
                                         const double&) = nullptr;
};

}  // namespace grmhd::ValenciaDivClean::fd
