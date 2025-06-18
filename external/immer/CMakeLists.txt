# Distributed under the OSI-approved BSD 3-Clause License. See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.15)

project(immer-download NONE)

include(ExternalProject)
ExternalProject_Add(immer_project
  URL "https://github.com/arximboldi/immer/archive/refs/tags/v0.7.0.zip"
  URL_HASH SHA1=eac139ae08d8f4281ca8c64977e679cfd3da2304
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/immer-0.7.0"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/immer-0.7.0-build"
  INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/immer-0.7.0-install"
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/immer-0.7.0-install
             -DCMAKE_BUILD_TYPE=Release
             -DIMMER_BUILD_TESTS=OFF
             -DIMMER_BUILD_EXAMPLES=OFF
  BUILD_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
)

# Set IMMER_INCLUDE_DIR after the project has been built and installed
ExternalProject_Get_Property(immer_project INSTALL_DIR)
# INSTALL_DIR is ${CMAKE_CURRENT_BINARY_DIR}/immer-0.7.0-install
# Headers will be in ${INSTALL_DIR}/include
set(IMMER_INCLUDE_DIR "${INSTALL_DIR}/include" CACHE INTERNAL "Immer include directory")
message(STATUS "Immer include directory: ${IMMER_INCLUDE_DIR}")
