# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/app/external/map/map_repo-prefix/src/map_repo"
  "/app/external/map/map_repo-prefix/src/map_repo-build"
  "/app/external/map/map_repo-prefix"
  "/app/external/map/map_repo-prefix/tmp"
  "/app/external/map/map_repo-prefix/src/map_repo-stamp"
  "/app/external/map/map_repo-prefix/src"
  "/app/external/map/map_repo-prefix/src/map_repo-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/app/external/map/map_repo-prefix/src/map_repo-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/app/external/map/map_repo-prefix/src/map_repo-stamp${cfgdir}") # cfgdir has leading slash
endif()
