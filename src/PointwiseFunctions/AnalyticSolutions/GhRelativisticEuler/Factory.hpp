// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include "PointwiseFunctions/AnalyticSolutions/GeneralRelativity/WrappedGr.hpp"
#include "PointwiseFunctions/AnalyticSolutions/RelativisticEuler/FishboneMoncriefDisk.hpp"
#include "PointwiseFunctions/AnalyticSolutions/RelativisticEuler/TovStar.hpp"
#include "Utilities/TMPL.hpp"

namespace gh::RelativisticEuler::Solutions {
/// Relativistic hydro solutions wrapped for GH

/// \brief List of all analytic solutions
using all_solutions = tmpl::list<
    gh::Solutions::WrappedGr<
        ::RelativisticEuler::Solutions::FishboneMoncriefDisk>,
    gh::Solutions::WrappedGr<::RelativisticEuler::Solutions::TovStar>>;

}  // namespace gh::RelativisticEuler::Solutions
