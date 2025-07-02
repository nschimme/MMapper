# MSIX Packaging Script for MMapper

# Ensure this script is only processed during the Windows build
if(NOT WIN32)
    return()
endif()

# This script is called by CPack's External generator.
# CPack is responsible for staging all necessary files for the MSIX_Package_Component
# into CPACK_TEMPORARY_DIRECTORY. This script then uses that directory as input.

set(APP_TARGET mmapper) # As determined from src/CMakeLists.txt

# Versioning: Use CPACK_PACKAGE_VERSION variables. These are set by CPack when it runs.
# Versioning: Use CPACK_PACKAGE_VERSION variables. These should be set by the main CMakeLists.txt
# CPack itself also sets CPACK_PACKAGE_VERSION_MAJOR etc. when it runs.
if(NOT DEFINED CPACK_PACKAGE_VERSION_MAJOR OR NOT DEFINED CPACK_PACKAGE_VERSION_MINOR OR NOT DEFINED CPACK_PACKAGE_VERSION_PATCH)
    # Fallback to project version if CPack vars aren't populated yet (e.g. if script is included directly)
    # However, for CPack External, CPack variables should be available.
    get_filename_component(PROJECT_VERSION_FROM_FILE "${CMAKE_SOURCE_DIR}/MMAPPER_VERSION" NAME)
    string(REPLACE "." ";" PROJECT_VERSION_LIST ${PROJECT_VERSION_FROM_FILE})
    list(GET PROJECT_VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
    list(GET PROJECT_VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
    list(GET PROJECT_VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)
    if(NOT DEFINED CPACK_PACKAGE_VERSION_MAJOR) # Still not defined? Error.
        message(FATAL_ERROR "MSIX: Package version information is not available.")
    endif()
    message(STATUS "MSIX: Using project version ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH} as fallback.")
endif()

set(APPX_BUILD_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.0")
set(APPX_VERSION_FILENAME_PART "${CPACK_PACKAGE_VERSION_MAJOR}_${CPACK_PACKAGE_VERSION_MINOR}_${CPACK_PACKAGE_VERSION_PATCH}_0")

# File and directory names for output
# Output .appxupload to CMAKE_BINARY_DIR as it's a persistent location outside CPack's temp staging.
set(APPX_PACKAGES_DIR_NAME "msix_intermediate_packages") # Subdirectory in CMAKE_BINARY_DIR for .appx
set(APPX_PACKAGE_X64_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}-x64.appx")
set(APPX_BUNDLE_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxbundle")
set(APPX_PDB_FILE_NAME "${APP_TARGET}.pdb")
set(APPX_SYM_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxsym")
set(APPX_UPLOAD_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxupload")

set(APPX_PACKAGES_FULL_DIR "${CMAKE_BINARY_DIR}/${APPX_PACKAGES_DIR_NAME}")
set(APPX_PACKAGE_X64_FULL_PATH "${APPX_PACKAGES_FULL_DIR}/${APPX_PACKAGE_X64_FILE_NAME}")
set(APPX_BUNDLE_FULL_PATH "${CMAKE_BINARY_DIR}/${APPX_BUNDLE_FILE_NAME}")

# Determine PDB path carefully
# For single-config generators (like Ninja on CI unless multi-config is specified):
set(PDB_SEARCH_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(PDB_SEARCH_PATH "${CMAKE_BINARY_DIR}") # Fallback
endif()
# For multi-config generators (like Visual Studio locally):
if(CMAKE_CFG_INTDIR AND NOT CMAKE_CFG_INTDIR STREQUAL ".")
    set(PDB_SEARCH_PATH "${PDB_SEARCH_PATH}/${CMAKE_CFG_INTDIR}")
endif()
set(APPX_PDB_FULL_PATH "${PDB_SEARCH_PATH}/${APPX_PDB_FILE_NAME}")

if(NOT (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message(STATUS "MSIX: WARNING - Build type is ${CMAKE_BUILD_TYPE}. PDB file for .appxsym might not be generated or found. For full symbols, use RelWithDebInfo or Debug.")
endif()

set(APPX_SYM_FULL_PATH "${CMAKE_BINARY_DIR}/${APPX_SYM_FILE_NAME}")
set(APPX_UPLOAD_FULL_PATH "${CMAKE_BINARY_DIR}/${APPX_UPLOAD_FILE_NAME}")

# This install(CODE) block is the main part executed by CPack External generator
set(install_code)
string(CONCAT install_code
    "message(STATUS \"MSIX (CPack External): Starting packaging process.\")\n"
    "message(STATUS \"MSIX (CPack External): Staging directory used by MakeAppx (CPACK_TEMPORARY_DIRECTORY): ${CPACK_TEMPORARY_DIRECTORY}\")\n"
    "message(STATUS \"MSIX (CPack External): Output directory for intermediate packages (APPX_PACKAGES_FULL_DIR): ${APPX_PACKAGES_FULL_DIR}\")\n"
    "message(STATUS \"MSIX (CPack External): Final .appxupload will be in (CMAKE_BINARY_DIR): ${CMAKE_BINARY_DIR}\")\n"
    "message(STATUS \"MSIX (CPack External): PDB file expected at (APPX_PDB_FULL_PATH): ${APPX_PDB_FULL_PATH}\")\n"
    "\n"
    "# Ensure output directories exist for intermediate packages\n"
    "file(MAKE_DIRECTORY \"${APPX_PACKAGES_FULL_DIR}\")\n"
    "\n"
    "# AppxManifest.xml is expected to be at the root of CPACK_TEMPORARY_DIRECTORY, staged by CPack.\n"
    "set(MANIFEST_PATH \"${CPACK_TEMPORARY_DIRECTORY}/AppxManifest.xml\")\n"
    "if(EXISTS \${MANIFEST_PATH})\n"
    "    message(STATUS \"MSIX: Updating version in \${MANIFEST_PATH} to ${APPX_BUILD_VERSION_STRING}\")\n"
    "    file(READ \${MANIFEST_PATH} MANIFEST_CONTENT)\n"
    "    string(REGEX REPLACE \"Version=\\\"[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\"\" \"Version=\\\"${APPX_BUILD_VERSION_STRING}\\\"\" MANIFEST_CONTENT \${MANIFEST_CONTENT})\n"
    "    file(WRITE \${MANIFEST_PATH} \"\${MANIFEST_CONTENT}\")\n"
    "else()\n"
    "    message(FATAL_ERROR \"MSIX: AppxManifest.xml not found at \${MANIFEST_PATH}. Check if it's part of MSIX_Package_Component and staged correctly by CPack.\")\n"
    "endif()\n"
    "\n"
    "message(STATUS \"MSIX: Removing previous packages and bundle from binary dir (if present)...\")\n"
    "file(REMOVE_RECURSE \"${APPX_PACKAGES_FULL_DIR}\")\n"
    "file(MAKE_DIRECTORY \"${APPX_PACKAGES_FULL_DIR}\")\n"
    "file(REMOVE \"${APPX_BUNDLE_FULL_PATH}\")\n"
    "file(REMOVE \"${APPX_SYM_FULL_PATH}\")\n"
    "file(REMOVE \"${APPX_UPLOAD_FULL_PATH}\")\n"
    "\n"
)

set(package_x64_command "MakeAppx.exe pack /d \\\"${CPACK_TEMPORARY_DIRECTORY}\\\" /p \\\"${APPX_PACKAGE_X64_FULL_PATH}\\\" /o") # Use CPACK_TEMPORARY_DIRECTORY
set(package_x64_execute_process "execute_process(COMMAND ${package_x64_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")
set(bundle_command "MakeAppx.exe bundle /d \\\"${APPX_PACKAGES_FULL_DIR}\\\" /p \\\"${APPX_BUNDLE_FULL_PATH}\\\" /o")
set(bundle_execute_process "execute_process(COMMAND ${bundle_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")

string(CONCAT install_code "${install_code}"
    "message(STATUS \"MSIX: Packaging 64-bit app (Input: ${CPACK_TEMPORARY_DIRECTORY}, Output: ${APPX_PACKAGE_X64_FULL_PATH})\")\n" # Use CPACK_TEMPORARY_DIRECTORY
    "${package_x64_execute_process}\n"
    "if(NOT _res EQUAL 0){message(FATAL_ERROR \"MSIX: MakeAppx.exe pack failed with \${_res}. Check logs.\")}\n"
    "message(STATUS \"MSIX: Bundling all packages (Input: ${APPX_PACKAGES_FULL_DIR}, Output: ${APPX_BUNDLE_FULL_PATH})\")\n"
    "${bundle_execute_process}\n"
    "if(NOT _res EQUAL 0){message(FATAL_ERROR \"MSIX: MakeAppx.exe bundle failed with \${_res}. Check logs.\")}\n"
    "\n"
)

set(compress_pdb_command "powershell.exe -NoProfile -Command \\\"Compress-Archive -Path '${APPX_PDB_FULL_PATH}' -DestinationPath '${APPX_SYM_FULL_PATH}.zip' -ErrorAction Stop -Force\\\"")
set(compress_pdb_execute_process "execute_process(COMMAND ${compress_pdb_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")
set(rename_appxsym_command "powershell.exe -NoProfile -Command \\\"Move-Item -Path '${APPX_SYM_FULL_PATH}.zip' -Destination '${APPX_SYM_FULL_PATH}' -ErrorAction Stop -Force\\\"")
set(rename_appxsym_execute_process "execute_process(COMMAND ${rename_appxsym_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")

set(create_appxupload_command "powershell.exe -NoProfile -Command \\\"Compress-Archive -Path '${APPX_BUNDLE_FULL_PATH}', '${APPX_SYM_FULL_PATH}' -DestinationPath '${APPX_UPLOAD_FULL_PATH}.zip' -ErrorAction Stop -Force\\\"")
set(create_appxupload_execute_process "execute_process(COMMAND ${create_appxupload_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")
set(rename_appxupload_command "powershell.exe -NoProfile -Command \\\"Move-Item -Path '${APPX_UPLOAD_FULL_PATH}.zip' -Destination '${APPX_UPLOAD_FULL_PATH}' -ErrorAction Stop -Force\\\"")
set(rename_appxupload_execute_process "execute_process(COMMAND ${rename_appxupload_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)")

string(CONCAT install_code "${install_code}"
    "if(NOT EXISTS \\\"${APPX_PDB_FULL_PATH}\\\")\n"
    "    message(STATUS \\\"MSIX: WARNING - PDB file not found at ${APPX_PDB_FULL_PATH}. Skipping .appxsym creation. .appxupload will not include symbols.\\\")\n"
    "    set(create_appxupload_no_symbols_command \\\"powershell.exe -NoProfile -Command \\\\\\\"Compress-Archive -Path '${APPX_BUNDLE_FULL_PATH}' -DestinationPath '${APPX_UPLOAD_FULL_PATH}.zip' -ErrorAction Stop -Force\\\\\\\"\\\")\n"
    "    set(create_appxupload_no_symbols_execute_process \\\"execute_process(COMMAND \${create_appxupload_no_symbols_command} RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)\\\")\n"
    "    message(STATUS \\\"MSIX: Creating .appxupload (without symbols)...\\\")\n"
    "    \${create_appxupload_no_symbols_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Compress-Archive for .appxupload (no symbols) failed: \${_res}\\\")}\n"
    "    \${rename_appxupload_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Move-Item for .appxupload (no symbols) failed: \${_res}\\\")}\n"
    "else()\n"
    "    message(STATUS \\\"MSIX: Converting .pdb to .appxsym (PDB: ${APPX_PDB_FULL_PATH}, Output: ${APPX_SYM_FULL_PATH})\\\")\n"
    "    \${compress_pdb_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Compress-Archive for PDB failed: \${_res}\\\")}\n"
    "    \${rename_appxsym_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Move-Item for .appxsym failed: \${_res}\\\")}\n"
    "    message(STATUS \\\"MSIX: Creating .appxupload (Bundle: ${APPX_BUNDLE_FULL_PATH}, Symbols: ${APPX_SYM_FULL_PATH}, Output: ${APPX_UPLOAD_FULL_PATH})\\\")\n"
    "    \${create_appxupload_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Compress-Archive for .appxupload failed: \${_res}\\\")}\n"
    "    \${rename_appxupload_execute_process}\n"
    "    if(NOT _res EQUAL 0){message(FATAL_ERROR \\\"MSIX: Move-Item for .appxupload failed: \${_res}\\\")}\n"
    "endif()\n"
    "message(STATUS \\\"MSIX: Packaging complete. Final upload file: ${APPX_UPLOAD_FULL_PATH}\\\")\n"
)

# This install(CODE) is executed by CPack when the 'External' generator runs.
# CPack sets up CMAKE_INSTALL_PREFIX to a temporary staging directory.
# The component MSIX_Package_Component should have been installed to this staging directory by CPack first.
install(CODE "${install_code}")

message(STATUS "MSIX: msix.cmake script configured for CPack External. Packaging will occur during CPack execution.")
