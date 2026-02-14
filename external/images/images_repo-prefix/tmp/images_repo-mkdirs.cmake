# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/app/images"
  "/app/external/images/images_repo-prefix/src/images_repo-build"
  "/app/external/images/images_repo-prefix"
  "/app/external/images/images_repo-prefix/tmp"
  "/app/external/images/images_repo-prefix/src/images_repo-stamp"
  "/app/external/images/images_repo-prefix/src"
  "/app/external/images/images_repo-prefix/src/images_repo-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/app/external/images/images_repo-prefix/src/images_repo-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/app/external/images/images_repo-prefix/src/images_repo-stamp${cfgdir}") # cfgdir has leading slash
endif()
