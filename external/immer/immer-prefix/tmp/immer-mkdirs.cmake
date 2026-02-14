# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/app/external/immer/immer"
  "/app/external/immer/immer-prefix/src/immer-build"
  "/app/external/immer/immer-prefix"
  "/app/external/immer/immer-prefix/tmp"
  "/app/external/immer/immer-prefix/src/immer-stamp"
  "/app/external/immer/immer-prefix/src"
  "/app/external/immer/immer-prefix/src/immer-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/app/external/immer/immer-prefix/src/immer-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/app/external/immer/immer-prefix/src/immer-stamp${cfgdir}") # cfgdir has leading slash
endif()
