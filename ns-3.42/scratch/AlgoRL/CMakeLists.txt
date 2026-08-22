# Each simulation script is built as a separate scratch executable

build_exec(
  EXECNAME d-lora-simulation
  SOURCE_FILES d-lora-simulation.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/AlgoRL
)

build_exec(
  EXECNAME lora-ucb1-simulation
  SOURCE_FILES lora-ucb1-simulation.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/AlgoRL
)

build_exec(
  EXECNAME lorawan-qoca-simulation
  SOURCE_FILES lorawan_qoca_simulation.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/AlgoRL
)

build_exec(
  EXECNAME tow-lorawan-simulation
  SOURCE_FILES tow-lorawan-simulation.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/AlgoRL
)

build_exec(
  EXECNAME unified-rl-simulation
  SOURCE_FILES unified-rl-simulation.cc
  LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
  EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/AlgoRL
)
