# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_Limiters")

set(LIBRARY_SOURCES
  Test_HwenoImpl.cpp
  Test_Krivodonova.cpp
  Test_LimiterActions.cpp
  Test_LimiterActionsWithMinmod.cpp
  Test_Minmod.cpp
  Test_MinmodTci.cpp
  Test_MinmodType.cpp
  Test_SimpleWenoImpl.cpp
  Test_Tags.cpp
  Test_Weno.cpp
  Test_WenoGridHelpers.cpp
  Test_WenoOscillationIndicator.cpp
  Test_WenoHelpers.cpp
  Test_WenoType.cpp
  )

add_test_library(${LIBRARY} "${LIBRARY_SOURCES}")

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  Boost::boost
  Limiters
  )
