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

# Paths for outputs will use CPACK_TOPLEVEL_DIRECTORY (the build dir where cpack was invoked)
set(APPX_PACKAGES_FULL_DIR "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_PACKAGES_DIR_NAME}")
set(APPX_PACKAGE_X64_FULL_PATH "${APPX_PACKAGES_FULL_DIR}/${APPX_PACKAGE_X64_FILE_NAME}")
set(APPX_BUNDLE_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_BUNDLE_FILE_NAME}") # Bundle goes to toplevel
set(APPX_SYM_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_SYM_FILE_NAME}")
set(APPX_UPLOAD_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_UPLOAD_FILE_NAME}")

# Determine CMAKE_BUILD_TYPE for PDB path logic (already done above this section in the current file)
# The CMAKE_BUILD_TYPE determined above will be used.

# PDB path, relative to CPACK_TOPLEVEL_DIRECTORY. From CI log: /pdb:src\mmapper.pdb
set(APPX_PDB_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/src/${APPX_PDB_FILE_NAME}") # Corrected base path

# --- Start of direct execution block (formerly install_code) ---

# Diagnostic: List contents of CPACK_TEMPORARY_DIRECTORY
message(STATUS "MSIX: Listing contents of CPACK_TEMPORARY_DIRECTORY (${CPACK_TEMPORARY_DIRECTORY}):")
execute_process(
    COMMAND cmd /c "dir \"${CPACK_TEMPORARY_DIRECTORY}\" /s /b"
    OUTPUT_VARIABLE _dir_listing
    ERROR_VARIABLE _dir_listing_error
    RESULT_VARIABLE _dir_listing_result
)
message(STATUS "MSIX: Directory listing output of CPACK_TEMPORARY_DIRECTORY:\n${_dir_listing}")
if(NOT _dir_listing_result EQUAL 0)
    message(WARNING "MSIX: Directory listing command failed or produced error:\n${_dir_listing_error}")
endif()
if(EXISTS "${CPACK_TEMPORARY_DIRECTORY}/AppxManifest.xml")
    message(STATUS "MSIX: AppxManifest.xml FOUND in CPACK_TEMPORARY_DIRECTORY by initial check.")
else()
    message(WARNING "MSIX: AppxManifest.xml NOT FOUND in CPACK_TEMPORARY_DIRECTORY by initial check.")
endif()

message(STATUS "MSIX (CPack External): Starting packaging process.")
message(STATUS "MSIX (CPack External): Staging directory used by MakeAppx (CPACK_TEMPORARY_DIRECTORY): ${CPACK_TEMPORARY_DIRECTORY}")
message(STATUS "MSIX (CPack External): Output directory for intermediate packages (APPX_PACKAGES_FULL_DIR): ${APPX_PACKAGES_FULL_DIR}")
message(STATUS "MSIX (CPack External): Final .appxupload will be in (CPACK_TOPLEVEL_DIRECTORY): ${CPACK_TOPLEVEL_DIRECTORY}") # Corrected variable
message(STATUS "MSIX (CPack External): PDB file expected at (APPX_PDB_FULL_PATH): ${APPX_PDB_FULL_PATH}")

# Ensure output directories exist for intermediate packages
file(MAKE_DIRECTORY "${APPX_PACKAGES_FULL_DIR}")

# AppxManifest.xml is expected to be at the root of CPACK_TEMPORARY_DIRECTORY, staged by CPack.
set(MANIFEST_PATH "${CPACK_TEMPORARY_DIRECTORY}/AppxManifest.xml")
if(EXISTS "${MANIFEST_PATH}")
    message(STATUS "MSIX: Updating version in ${MANIFEST_PATH} to ${APPX_BUILD_VERSION_STRING}")
    file(READ "${MANIFEST_PATH}" MANIFEST_CONTENT)
    string(REGEX REPLACE "Version=\\\"[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\\.[0-9]+\\\"" "Version=\\\"${APPX_BUILD_VERSION_STRING}\\\"" MANIFEST_CONTENT "${MANIFEST_CONTENT}")
    file(WRITE "${MANIFEST_PATH}" "${MANIFEST_CONTENT}")
else()
    message(FATAL_ERROR "MSIX: AppxManifest.xml not found at ${MANIFEST_PATH}. Check if it's part of MSIX_Assets component and staged correctly by CPack.")
endif()

message(STATUS "MSIX: Removing previous packages and bundle from binary dir (if present)...")
file(REMOVE_RECURSE "${APPX_PACKAGES_FULL_DIR}")
file(MAKE_DIRECTORY "${APPX_PACKAGES_FULL_DIR}") # Recreate it
file(REMOVE "${APPX_BUNDLE_FULL_PATH}")
file(REMOVE "${APPX_SYM_FULL_PATH}")
file(REMOVE "${APPX_UPLOAD_FULL_PATH}")

# Package and bundle commands
set(package_x64_command "MakeAppx.exe pack /d \"${CPACK_TEMPORARY_DIRECTORY}\" /p \"${APPX_PACKAGE_X64_FULL_PATH}\" /o") # Use CPACK_TEMPORARY_DIRECTORY
execute_process(COMMAND ${package_x64_command} RESULT_VARIABLE _res_pack OUTPUT_QUIET ERROR_QUIET)
message(STATUS "MSIX: Packaging 64-bit app (Input: ${CPACK_TEMPORARY_DIRECTORY}, Output: ${APPX_PACKAGE_X64_FULL_PATH})")
if(NOT _res_pack EQUAL 0)
    message(FATAL_ERROR "MSIX: MakeAppx.exe pack failed with ${_res_pack}. Check logs.")
endif()

set(bundle_command "MakeAppx.exe bundle /d \"${APPX_PACKAGES_FULL_DIR}\" /p \"${APPX_BUNDLE_FULL_PATH}\" /o")
execute_process(COMMAND ${bundle_command} RESULT_VARIABLE _res_bundle OUTPUT_QUIET ERROR_QUIET)
message(STATUS "MSIX: Bundling all packages (Input: ${APPX_PACKAGES_FULL_DIR}, Output: ${APPX_BUNDLE_FULL_PATH})")
if(NOT _res_bundle EQUAL 0)
    message(FATAL_ERROR "MSIX: MakeAppx.exe bundle failed with ${_res_bundle}. Check logs.")
endif()

# Commands for .pdb to .appxsym conversion and .appxupload creation
if(NOT EXISTS "${APPX_PDB_FULL_PATH}")
    message(STATUS "MSIX: WARNING - PDB file not found at ${APPX_PDB_FULL_PATH}. Skipping .appxsym creation. .appxupload will not include symbols.")
    set(create_appxupload_final_command "powershell.exe -NoProfile -Command \"Compress-Archive -Path '${APPX_BUNDLE_FULL_PATH}' -DestinationPath '${APPX_UPLOAD_FULL_PATH}.zip' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${create_appxupload_final_command} RESULT_VARIABLE _res_upload_zip OUTPUT_QUIET ERROR_QUIET)
    message(STATUS "MSIX: Creating .appxupload (without symbols)...")
    if(NOT _res_upload_zip EQUAL 0)
        message(FATAL_ERROR "MSIX: Compress-Archive for .appxupload (no symbols) failed: ${_res_upload_zip}")
    endif()

    set(rename_appxupload_final_command "powershell.exe -NoProfile -Command \"Move-Item -Path '${APPX_UPLOAD_FULL_PATH}.zip' -Destination '${APPX_UPLOAD_FULL_PATH}' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${rename_appxupload_final_command} RESULT_VARIABLE _res_upload_rename OUTPUT_QUIET ERROR_QUIET)
    if(NOT _res_upload_rename EQUAL 0)
        message(FATAL_ERROR "MSIX: Move-Item for .appxupload (no symbols) failed: ${_res_upload_rename}")
    endif()
else()
    message(STATUS "MSIX: Converting .pdb to .appxsym (PDB: ${APPX_PDB_FULL_PATH}, Output: ${APPX_SYM_FULL_PATH})")
    set(compress_pdb_final_command "powershell.exe -NoProfile -Command \"Compress-Archive -Path '${APPX_PDB_FULL_PATH}' -DestinationPath '${APPX_SYM_FULL_PATH}.zip' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${compress_pdb_final_command} RESULT_VARIABLE _res_pdb_zip OUTPUT_QUIET ERROR_QUIET)
    if(NOT _res_pdb_zip EQUAL 0)
        message(FATAL_ERROR "MSIX: Compress-Archive for PDB failed: ${_res_pdb_zip}")
    endif()

    set(rename_appxsym_final_command "powershell.exe -NoProfile -Command \"Move-Item -Path '${APPX_SYM_FULL_PATH}.zip' -Destination '${APPX_SYM_FULL_PATH}' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${rename_appxsym_final_command} RESULT_VARIABLE _res_pdb_rename OUTPUT_QUIET ERROR_QUIET)
    if(NOT _res_pdb_rename EQUAL 0)
        message(FATAL_ERROR "MSIX: Move-Item for .appxsym failed: ${_res_pdb_rename}")
    endif()

    message(STATUS "MSIX: Creating .appxupload (Bundle: ${APPX_BUNDLE_FULL_PATH}, Symbols: ${APPX_SYM_FULL_PATH}, Output: ${APPX_UPLOAD_FULL_PATH})")
    set(create_appxupload_final_command_with_symbols "powershell.exe -NoProfile -Command \"Compress-Archive -Path '${APPX_BUNDLE_FULL_PATH}', '${APPX_SYM_FULL_PATH}' -DestinationPath '${APPX_UPLOAD_FULL_PATH}.zip' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${create_appxupload_final_command_with_symbols} RESULT_VARIABLE _res_upload_zip_sym OUTPUT_QUIET ERROR_QUIET)
    if(NOT _res_upload_zip_sym EQUAL 0)
        message(FATAL_ERROR "MSIX: Compress-Archive for .appxupload failed: ${_res_upload_zip_sym}")
    endif()

    set(rename_appxupload_final_command_sym "powershell.exe -NoProfile -Command \"Move-Item -Path '${APPX_UPLOAD_FULL_PATH}.zip' -Destination '${APPX_UPLOAD_FULL_PATH}' -ErrorAction Stop -Force\"")
    execute_process(COMMAND ${rename_appxupload_final_command_sym} RESULT_VARIABLE _res_upload_rename_sym OUTPUT_QUIET ERROR_QUIET)
    if(NOT _res_upload_rename_sym EQUAL 0)
        message(FATAL_ERROR "MSIX: Move-Item for .appxupload failed: ${_res_upload_rename_sym}")
    endif()
endif()

message(STATUS "MSIX: Packaging complete. Final upload file: ${APPX_UPLOAD_FULL_PATH}")
# --- End of direct execution block ---

message(STATUS "MSIX: msix.cmake script configured for CPack External. Packaging will occur during CPack execution.")
