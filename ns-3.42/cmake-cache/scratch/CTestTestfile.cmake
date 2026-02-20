# CMake generated Testfile for 
# Source directory: /home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch
# Build directory: /home/bonice/Documents/ns-allinone-3.42/ns-3.42/cmake-cache/scratch
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ctest-scratch_lorawan-adr-simulation-module "ns3.42-lorawan-adr-simulation-module-default")
set_tests_properties(ctest-scratch_lorawan-adr-simulation-module PROPERTIES  WORKING_DIRECTORY "/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build/scratch/" _BACKTRACE_TRIPLES "/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build-support/custom-modules/ns3-executables.cmake;47;add_test;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build-support/custom-modules/ns3-executables.cmake;140;set_runtime_outputdirectory;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;67;build_exec;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;79;create_scratch;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;0;")
add_test(ctest-scratch_subdir_scratch-subdir "ns3.42-scratch-subdir-default")
set_tests_properties(ctest-scratch_subdir_scratch-subdir PROPERTIES  WORKING_DIRECTORY "/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build/scratch/subdir/" _BACKTRACE_TRIPLES "/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build-support/custom-modules/ns3-executables.cmake;47;add_test;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/build-support/custom-modules/ns3-executables.cmake;140;set_runtime_outputdirectory;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;67;build_exec;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;109;create_scratch;/home/bonice/Documents/ns-allinone-3.42/ns-3.42/scratch/CMakeLists.txt;0;")
subdirs("nested-subdir")
