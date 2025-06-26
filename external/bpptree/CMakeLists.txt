# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2023 The MMapper Authors

# This CMakeLists.txt uses ExternalProject_Add to fetch BppTree
# similar to how 'immer' is handled.

include(ExternalProject)

ExternalProject_Add(bpptree
    GIT_REPOSITORY "https://github.com/jeffplaisance/BppTree.git"
    GIT_TAG "main"  # Consider using a specific commit hash or tag for reproducibility
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bpptree"
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    ALWAYS 0
)

ExternalProject_Get_Property(bpptree SOURCE_DIR)

# The BppTree repository structure is:
# BppTree/
#   bpptree/
#     bpptree.h
#     detail/
#       ...
# The actual headers are in BppTree/include/bpptree/bpptree.hpp
# So, to include with <bpptree/bpptree.hpp>, the include path needs to point to BppTree/include.
set(BPPTREE_INCLUDE_DIR "${SOURCE_DIR}/include" CACHE INTERNAL "Path to BppTree include directory")
# Make sure this variable is available to the parent scope (main CMakeLists)
set(BPPTREE_INCLUDE_DIR ${BPPTREE_INCLUDE_DIR} PARENT_SCOPE)

set(BPPTREE_FOUND TRUE PARENT_SCOPE)
