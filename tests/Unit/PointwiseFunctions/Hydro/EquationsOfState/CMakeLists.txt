# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_EquationsOfState")

set(LIBRARY_SOURCES
  Test_Barotropic3D.cpp
  Test_DarkEnergyFluid.cpp
  Test_Enthalpy.cpp
  Test_Equilibrium3D.cpp
  Test_HybridEos.cpp
  Test_IdealFluid.cpp
  Test_PiecewisePolytropicFluid.cpp
  Test_PolytropicFluid.cpp
  Test_SpectralEoS.cpp
  Test_Tabulated3D.cpp
  )

add_test_library(${LIBRARY} "${LIBRARY_SOURCES}")

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  DataStructures
  Hydro
  )

add_subdirectory(Python)
