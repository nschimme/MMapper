# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2023 The MMapper Authors

# This CMakeLists.txt uses ExternalProject_Add to fetch BppTree
# similar to how 'immer' is handled.

include(ExternalProject)

# Give a unique name to the ExternalProject target for BppTree
set(BPPTREE_PROJECT_NAME bpptree_external)

ExternalProject_Add(${BPPTREE_PROJECT_NAME}
    GIT_REPOSITORY "https://github.com/jeffplaisance/BppTree.git"
    GIT_TAG "master"  # Consider using a specific commit hash or tag for reproducibility
    SOURCE_DIR "${CMAKE_BINARY_DIR}/bpptree_src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/bpptree_build" # Not strictly needed for header-only
    CONFIGURE_COMMAND "" # Header-only, no configure step
    BUILD_COMMAND ""     # Header-only, no build step
    INSTALL_COMMAND ""   # Header-only, no install step
    UPDATE_COMMAND ""    # Avoid re-downloading unless necessary / explicitly told
    LOG_DOWNLOAD TRUE
    LOG_CONFIGURE FALSE # No configure command
    LOG_BUILD FALSE     # No build command
    LOG_INSTALL FALSE   # No install command
)

# The BppTree repository structure is:
# BppTree/
#   bpptree/
#     bpptree.h
#     detail/
#       ...
# The actual headers are in BppTree/include/bpptree/bpptree.hpp
# So, to include with <bpptree/bpptree.hpp>, the include path needs to point to BppTree/include.
set(BPPTREE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/bpptree_src/include" CACHE INTERNAL "Path to BppTree include directory")
# Make sure this variable is available to the parent scope (main CMakeLists)
set(BPPTREE_INCLUDE_DIR ${BPPTREE_INCLUDE_DIR} PARENT_SCOPE)

# Create an interface library for BppTree
add_library(bpptree_interface INTERFACE)
target_include_directories(bpptree_interface INTERFACE
    "$<BUILD_INTERFACE:${BPPTREE_INCLUDE_DIR}>"
    # No install interface for now, as it's typically handled by the main project's install rules
)

# Add a dependency from the interface library to the external project target
# This ensures that bpptree_external is built (i.e., downloaded) before anything
# that links to bpptree_interface tries to compile.
add_dependencies(bpptree_interface ${BPPTREE_PROJECT_NAME})

set(BPPTREE_FOUND TRUE PARENT_SCOPE)

message(STATUS "Configuring BppTree: Will be downloaded via ExternalProject to ${BPPTREE_INCLUDE_DIR}")
message(STATUS "BppTree project target: ${BPPTREE_PROJECT_NAME}")
message(STATUS "BppTree interface library: bpptree_interface")
message(STATUS "To use BppTree, #include <bpptree/bpptree.h>")
