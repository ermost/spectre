# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_ParallelAlgorithmsActions")

set(LIBRARY_SOURCES
  Test_AddComputeTags.cpp
  Test_AddSimpleTags.cpp
  Test_Goto.cpp
  Test_InitializeItems.cpp
  Test_MutateApply.cpp
  Test_RandomizeVariables.cpp
  Test_SetData.cpp
  Test_TerminatePhase.cpp
  Test_UpdateMessageQueue.cpp
  )

add_test_library(${LIBRARY} "${LIBRARY_SOURCES}")

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  DataStructures
  DomainStructure
  )
