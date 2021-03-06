cmake_minimum_required(VERSION 3.10)

project(votca-csg LANGUAGES CXX)

set(PROJECT_VERSION "1.7-dev")
set(PROJECT_CONTACT "bugs@votca.org")
string(REGEX REPLACE "^[1-9]+\\.([1-9]+).*$" "\\1" SOVERSION "${PROJECT_VERSION}")
if (NOT ${SOVERSION} MATCHES "[1-9]+")
  message(FATAL_ERROR "Could not determind SOVERSION from ${PROJECT_VERSION}")
endif (NOT ${SOVERSION} MATCHES "[1-9]+")

# Cmake modules/macros are in a subdirectory to keep this file cleaner
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CXX_FLAGS)
  #Release comes with -O3 by default
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CXX_FLAGS)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DDEBUG)
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

######################################################################
# compiler tests
# these need ot be done early (before further tests).
#####################################################################

include(CheckCXXCompilerFlag)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) #...without compiler extensions like gnu++11

########################################################################
# User input options                                                   #
########################################################################
option(BUILD_SHARED_LIBS "Build shared libs" ON)
include(GNUInstallDirs)
include(FeatureSummary)

option(ENABLE_TESTING "Build and enable testing stuff" OFF)
add_feature_info(ENABLE_TESTING ENABLE_TESTING "Enable unit tests")
if(ENABLE_TESTING)
  enable_testing()
endif(ENABLE_TESTING)

########################################################################
#Find external packages
########################################################################
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  find_package(Git)
  set_package_properties(Git PROPERTIES TYPE OPTIONAL PURPOSE "Generated version for development version")
endif(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.git)

option(BUILD_MANPAGES "Build manpages" ON)
add_feature_info(BUILD_MANPAGES BUILD_MANPAGES "Build manpages (disable for cross-compile)")
if (BUILD_MANPAGES)
  find_package(TXT2TAGS)
  set_package_properties(TXT2TAGS PROPERTIES TYPE OPTIONAL PURPOSE "Used to build manpages")
  find_package(UnixCommands)
  set_package_properties(UnixCommands PROPERTIES TYPE OPTIONAL PURPOSE "Used to build manpages")
else (BUILD_MANPAGES)
  #we use TXT2TAGS_FOUND conditionals in other CMakeLists.txt
  #so that TXT2TAGS is never required
  set(TXT2TAGS_FOUND)
endif (BUILD_MANPAGES)

find_package(Boost 1.53.0 REQUIRED COMPONENTS program_options filesystem system regex)
find_package(Eigen3 3.3.0 NO_MODULE REQUIRED)
find_package(Threads REQUIRED)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set_package_properties(Threads PROPERTIES TYPE REQUIRED PURPOSE "Used for thread parallelization")
  set_package_properties(Boost PROPERTIES TYPE REQUIRED PURPOSE "Extended C++ libraries")
  set_package_properties(Eigen3 PROPERTIES TYPE REQUIRED PURPOSE "C++ vector data structures")
endif()
message(STATUS "Found Eigen3: ${Eigen3_DIR}")

find_package(VOTCA_TOOLS NO_MODULE REQUIRED)
set_package_properties(VOTCA_TOOLS PROPERTIES TYPE REQUIRED PURPOSE "Votca base library")
message(STATUS "Found VOTCA_TOOLS: ${VOTCA_TOOLS_DIR}")
if(DEFINED VOTCA_TOOLS_VERSION AND NOT VOTCA_TOOLS_VERSION STREQUAL PROJECT_VERSION)
  message(FATAL_ERROR "Incompatible VOTCA Tools version found (needed ${PROJECT_VERSION}, found ${VOTCA_TOOLS_VERSION})")
endif()

find_package(GROMACS 2021 QUIET CONFIG NAMES gromacs gromacs_d)
set_package_properties(GROMACS PROPERTIES TYPE RECOMMENDED PURPOSE "Used to read/write gromacs data files")
if(NOT GROMACS_FOUND)
  find_package(GROMACS 2018 MODULE)
endif()
if(GROMACS_FOUND AND DEFINED GROMACS_VERSION AND GROMACS_VERSION VERSION_GREATER_EQUAL "2020")
  message(WARNING "Gromacs-2020 and above have no support for tabulated interactions, that are needed for coarse-graining (see and comment on https://gitlab.com/gromacs/gromacs/-/issues/1347)")
endif()

find_program(GMX_EXECUTABLE NAMES gmx_d gmx)
find_package_handle_standard_args(GMX REQUIRED_VARS GMX_EXECUTABLE)

if(ENABLE_TESTING)
  set(INTEGRATIONTEST_TOLERANCE "5e-5" CACHE STRING "Tolerance for the regression tests")
  mark_as_advanced(INTEGRATIONTEST_TOLERANCE)
  find_package(UnixCommands)
  set_package_properties(UnixCommands PROPERTIES TYPE OPTIONAL PURPOSE "Used in testing")
endif(ENABLE_TESTING)

########################################################################
# Basic system tests (standard libraries, headers, functions, types)   #
########################################################################
include(CheckIncludeFileCXX)
foreach(HEADER algorithm cassert fstream iomanip iostream list map numeric sstream stdexcept string vector cstdlib)
  check_include_file_cxx(${HEADER} FOUND_${HEADER})
  if(NOT FOUND_${HEADER})
    message(FATAL_ERROR "Could not find needed header - ${HEADER}")
  endif(NOT FOUND_${HEADER})
endforeach(HEADER)

set(MATH_LIBRARIES "m" CACHE STRING "math library")
mark_as_advanced( MATH_LIBRARIES )

######################################
# Include the following subdirectory # 
######################################
if(NOT TARGET manpages)
  add_custom_target(manpages ALL)
endif()
add_subdirectory(src)
add_subdirectory(scripts)
add_subdirectory(include/votca/csg)
add_subdirectory(share)

# needs to happen after src and scripts subdir
get_property(VOTCA_BINARIES TARGET votca_csg PROPERTY BINARIES)
list(SORT VOTCA_BINARIES)
configure_file(src/libcsg/VOTCA_CSGConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/src/libcsg/VOTCA_CSGConfig.cmake" @ONLY)
# for >=cmake-3.12 use list(JOIN VOTCA_BINARIES " " VOTCA_BINARIES)
string(REPLACE ";" " " VOTCA_BINARIES "${VOTCA_BINARIES}")
configure_file(scripts/csg-completion.bash.in "${CMAKE_CURRENT_BINARY_DIR}/scripts/csg-completion.bash" @ONLY)

configure_file(${PROJECT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake IMMEDIATE @ONLY)
add_custom_target(uninstall-csg COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
if(NOT TARGET uninstall)
  add_custom_target(uninstall)
endif()
add_dependencies(uninstall uninstall-csg)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  feature_summary(INCLUDE_QUIET_PACKAGES WHAT ALL)
endif (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
