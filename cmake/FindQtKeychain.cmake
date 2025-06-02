# Find QtKeychain
# ~~~~~~~~~~~~~~~
# Copyright (c) 2016, Boundless Spatial
# Author: Larry Shaffer <lshaffer (at) boundlessgeo (dot) com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for QtKeychain library from:
#    https://github.com/frankosterfeld/qtkeychain
#
# If it's found it sets QTKEYCHAIN_FOUND to TRUE
# and following variables are set:
#    QTKEYCHAIN_INCLUDE_DIR
#    QTKEYCHAIN_LIBRARY

FIND_PATH(QTKEYCHAIN_INCLUDE_DIR keychain.h
  PATHS
  ${LIB_DIR}/include
  "$ENV{LIB_DIR}/include"
  $ENV{INCLUDE}
  /usr/local/include
  /usr/include
  /app/include
  # Prioritize qt6keychain, then qt5keychain, then generic qtkeychain for include directories
  PATH_SUFFIXES qt6keychain qt5keychain qtkeychain
)

# Prioritize qt6keychain, then qt5keychain, then generic qtkeychain for libraries
FIND_LIBRARY(QTKEYCHAIN_LIBRARY NAMES qt6keychain qt5keychain qtkeychain
  PATHS
  ${LIB_DIR}
  "$ENV{LIB_DIR}"
  $ENV{LIB_DIR}/lib
  $ENV{LIB}
  /usr/local/lib
  /usr/lib
)

# The USE_QT6 conditional logic and fallback mechanism are no longer needed
# as the FIND_LIBRARY now searches in preferred order.

IF (QTKEYCHAIN_INCLUDE_DIR AND QTKEYCHAIN_LIBRARY)
  SET(QTKEYCHAIN_FOUND TRUE)
ELSE()
  SET(QTKEYCHAIN_FOUND FALSE)
ENDIF (QTKEYCHAIN_INCLUDE_DIR AND QTKEYCHAIN_LIBRARY)

IF (QTKEYCHAIN_FOUND)
   IF (NOT QTKEYCHAIN_FIND_QUIETLY)
      # Message updated to reflect that MMAPPER_ACTUAL_QT_VERSION from the main CMakeLists.txt
      # will determine which Qt version is actually linked against if keychain has variants.
      MESSAGE(STATUS "Found QtKeychain: ${QTKEYCHAIN_LIBRARY}. Linkage will depend on selected Qt version (${MMAPPER_ACTUAL_QT_VERSION}).")
   ENDIF (NOT QTKEYCHAIN_FIND_QUIETLY)
ELSE (QTKEYCHAIN_FOUND)
   IF (QTKEYCHAIN_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find QtKeychain. Searched for qt6keychain, qt5keychain, and qtkeychain.")
   ENDIF (QTKEYCHAIN_FIND_REQUIRED)
ENDIF (QTKEYCHAIN_FOUND)
