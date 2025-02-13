# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY ParallelLinearSolver)

add_spectre_library(${LIBRARY} INTERFACE)

spectre_target_headers(
  ${LIBRARY}
  INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/src
  HEADERS
  Observe.hpp
  Tags.hpp
  )

target_link_libraries(
  ${LIBRARY}
  INTERFACE
  Convergence
  DataStructures
  Initialization
  LinearSolver
  Logging
  Observer
  Parallel
  Serialization
  SystemUtilities
  Utilities
  )

add_subdirectory(Actions)
add_subdirectory(AsynchronousSolvers)
add_subdirectory(ConjugateGradient)
add_subdirectory(Gmres)
add_subdirectory(Multigrid)
add_subdirectory(Richardson)
add_subdirectory(Schwarz)
