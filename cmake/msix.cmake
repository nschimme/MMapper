# MSIX Packaging Script for MMapper

# Ensure this script is only processed during the Windows build
if(NOT WIN32)
    return()
endif()

# This script is called by CPack's External generator.
# It uses the pre-staged files from the install tree

set(APP_TARGET mmapper) # As determined from src/CMakeLists.txt

# Versioning: Determine the MSIX version (Major.Minor.Patch.Build)
# Prefer CPACK_PACKAGE_VERSION if it's in a recognizable format (simple or Git-based).
# Fallback to MMAPPER_VERSION if CPACK_PACKAGE_VERSION is not suitable or defined.

set(MSIX_MAJOR_VERSION 0)
set(MSIX_MINOR_VERSION 0)
set(MSIX_PATCH_VERSION 0)
set(MSIX_BUILD_VERSION 0)
set(VERSION_SOURCE "Unknown")

if(DEFINED CPACK_PACKAGE_VERSION)
    # Try matching simple Major.Minor.Patch first (e.g., for release tags)
    if(CPACK_PACKAGE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(MSIX_MAJOR_VERSION ${CMAKE_MATCH_1})
        set(MSIX_MINOR_VERSION ${CMAKE_MATCH_2})
        set(MSIX_PATCH_VERSION ${CMAKE_MATCH_3})
        set(MSIX_BUILD_VERSION 0) # Default build number for simple versions
        set(VERSION_SOURCE "CPACK_PACKAGE_VERSION (Simple)")
    # Then try matching Git-based Major.Minor.Patch-Commits-gHash
    elseif(CPACK_PACKAGE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g[0-9a-f]+$")
        set(MSIX_MAJOR_VERSION ${CMAKE_MATCH_1})
        set(MSIX_MINOR_VERSION ${CMAKE_MATCH_2})
        set(MSIX_PATCH_VERSION ${CMAKE_MATCH_3})
        set(MSIX_BUILD_VERSION ${CMAKE_MATCH_4}) # Use commit count as build number
        set(VERSION_SOURCE "CPACK_PACKAGE_VERSION (Git)")
    else()
        message(WARNING "MSIX: CPACK_PACKAGE_VERSION '${CPACK_PACKAGE_VERSION}' is defined but does not match expected formats (Major.Minor.Patch or Git-based). Falling back to MMAPPER_VERSION.")
        set(VERSION_SOURCE "CPACK_PACKAGE_VERSION (Unmatched) -> MMAPPER_VERSION")
    endif()
else()
    message(STATUS "MSIX: CPACK_PACKAGE_VERSION is not defined. Falling back to MMAPPER_VERSION.")
    set(VERSION_SOURCE "MMAPPER_VERSION")
endif()

# If version components are still 0 (meaning CPACK_PACKAGE_VERSION wasn't suitable or defined),
# try to get them from the MMAPPER_VERSION file.
if(${MSIX_MAJOR_VERSION} EQUAL 0 AND ${MSIX_MINOR_VERSION} EQUAL 0 AND ${MSIX_PATCH_VERSION} EQUAL 0)
    get_filename_component(PROJECT_VERSION_FROM_FILE "${CMAKE_SOURCE_DIR}/MMAPPER_VERSION" NAME)
    if(PROJECT_VERSION_FROM_FILE MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(MSIX_MAJOR_VERSION ${CMAKE_MATCH_1})
        set(MSIX_MINOR_VERSION ${CMAKE_MATCH_2})
        set(MSIX_PATCH_VERSION ${CMAKE_MATCH_3})
        set(MSIX_BUILD_VERSION 0) # Default build number for file-based versions
        if(VERSION_SOURCE STREQUAL "MMAPPER_VERSION")
             message(STATUS "MSIX: Using file-based version ${MSIX_MAJOR_VERSION}.${MSIX_MINOR_VERSION}.${MSIX_PATCH_VERSION}.${MSIX_BUILD_VERSION} from MMAPPER_VERSION (${PROJECT_VERSION_FROM_FILE}).")
        else()
             # This case happens if CPACK_PACKAGE_VERSION was defined but unmatched, and we successfully parsed MMAPPER_VERSION
             message(STATUS "MSIX: Successfully parsed file-based version ${MSIX_MAJOR_VERSION}.${MSIX_MINOR_VERSION}.${MSIX_PATCH_VERSION}.${MSIX_BUILD_VERSION} from MMAPPER_VERSION (${PROJECT_VERSION_FROM_FILE}) after CPACK_PACKAGE_VERSION mismatch.")
        endif()
    else()
        message(FATAL_ERROR "MSIX: Could not determine package version from CPACK_PACKAGE_VERSION or MMAPPER_VERSION file.")
    endif()
endif()

set(APPX_BUILD_VERSION_STRING "${MSIX_MAJOR_VERSION}.${MSIX_MINOR_VERSION}.${MSIX_PATCH_VERSION}.${MSIX_BUILD_VERSION}")
set(APPX_VERSION_FILENAME_PART "${MSIX_MAJOR_VERSION}_${MSIX_MINOR_VERSION}_${MSIX_PATCH_VERSION}_${MSIX_BUILD_VERSION}")

message(STATUS "MSIX: Final package version string: ${APPX_BUILD_VERSION_STRING}")
message(STATUS "MSIX: Final package filename part: ${APPX_VERSION_FILENAME_PART}")

# File and directory names for output
set(APPX_PACKAGES_DIR_NAME "msix_intermediate_packages")
set(APPX_PACKAGE_X64_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}-x64.appx")
set(APPX_BUNDLE_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxbundle")
set(APPX_PDB_FILE_NAME "${APP_TARGET}.pdb") # This is just the filename, path is separate
set(APPX_SYM_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxsym")
set(APPX_UPLOAD_FILE_NAME "${APP_TARGET}-${APPX_VERSION_FILENAME_PART}.appxupload")

# Paths for final outputs will use CPACK_TOPLEVEL_DIRECTORY (the build dir where cpack was invoked for this script)
set(APPX_PACKAGES_FULL_DIR "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_PACKAGES_DIR_NAME}") # Intermediate packages
set(APPX_PACKAGE_X64_FULL_PATH "${APPX_PACKAGES_FULL_DIR}/${APPX_PACKAGE_X64_FILE_NAME}")
set(APPX_BUNDLE_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_BUNDLE_FILE_NAME}") # Intermediate bundle
set(APPX_SYM_FULL_PATH "${CPACK_TOPLEVEL_DIRECTORY}/${APPX_SYM_FILE_NAME}")       # Intermediate symbols

# CPACK_TOPLEVEL_DIRECTORY for external script is .../artifact/_CPack_Packages/Windows/External/
# We want the final .appxupload to be in .../artifact/
get_filename_component(MAIN_ARTIFACT_DIR "${CPACK_TOPLEVEL_DIRECTORY}/../../../" ABSOLUTE CLEAN)
set(APPX_UPLOAD_FULL_PATH "${MAIN_ARTIFACT_DIR}/${APPX_UPLOAD_FILE_NAME}")
message(STATUS "MSIX: Adjusted final .appxupload output path to: ${APPX_UPLOAD_FULL_PATH}")

# Source PDB path: Located in the CPack temporary install directory
set(APPX_PDB_SOURCE_PATH "${CPACK_TEMPORARY_DIRECTORY}/${APPX_PDB_FILE_NAME}")

# The input directory for MakeAppx.exe is the pre-staged install tree
message(STATUS "MSIX: Input directory for MakeAppx.exe set to: ${CPACK_TEMPORARY_DIRECTORY}")

message(STATUS "DEBUG: Running msix.configured.cmake")
message(STATUS "DEBUG: Configured CMAKE_SOURCE_DIR is: ${CMAKE_SOURCE_DIR}")

# --- Start of direct execution block ---

# Diagnostic: List contents of the input directory (CPACK_TEMPORARY_DIRECTORY)
message(STATUS "MSIX: Contents of input directory (${CPACK_TEMPORARY_DIRECTORY}) before packaging:")
file(GLOB_RECURSE _input_files LIST_DIRECTORIES true FOLLOW_SYMLINKS
     RELATIVE "${CPACK_TEMPORARY_DIRECTORY}"
     "${CPACK_TEMPORARY_DIRECTORY}/*"
)
if(_input_files)
    foreach(_file ${_input_files})
        message(STATUS "  -- ${_file}")
    endforeach()
else()
    message(WARNING "MSIX: Input directory (${CPACK_TEMPORARY_DIRECTORY}) appears empty or contains no enumerable items.")
endif()

# Move contents of 'applications' and 'libraries' to staging root
message(STATUS "MSIX: Moving staged files to staging root...")

# Move contents of 'applications'
file(GLOB applications_contents "${CPACK_TEMPORARY_DIRECTORY}/applications/*")
foreach(item ${applications_contents})
    get_filename_component(item_name "${item}" NAME)
    file(RENAME "${item}" "${CPACK_TEMPORARY_DIRECTORY}/${item_name}")
endforeach()

# Move contents of 'libraries'
file(GLOB libraries_contents "${CPACK_TEMPORARY_DIRECTORY}/libraries/*")
foreach(item ${libraries_contents})
    get_filename_component(item_name "${item}" NAME)
    file(RENAME "${item}" "${CPACK_TEMPORARY_DIRECTORY}/${item_name}")
endforeach()

# Remove the now-empty directories
file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/applications")
file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/libraries")

message(STATUS "MSIX: Move complete.")

# Find MakeAppx.exe
find_program(MAKEAPPX_EXE MakeAppx.exe
    HINTS
        # Common location for Windows Kits if ProgramFiles(x86) is defined
        # find_program will search common subdirectories like specific SDK versions/x64 or /x86
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin"
    REQUIRED
)
message(STATUS "MSIX: Using MakeAppx.exe found at: ${MAKEAPPX_EXE}")

message(STATUS "MSIX: Removing previous packages and bundle from output dir (if present)...")
file(REMOVE_RECURSE "${APPX_PACKAGES_FULL_DIR}") # Clean intermediate dir
file(MAKE_DIRECTORY "${APPX_PACKAGES_FULL_DIR}") # Recreate it
file(REMOVE "${APPX_BUNDLE_FULL_PATH}")
file(REMOVE "${APPX_SYM_FULL_PATH}")
file(REMOVE "${APPX_UPLOAD_FULL_PATH}")

# Package and bundle commands
execute_process(
    COMMAND "${MAKEAPPX_EXE}"
    pack
    /d "${CPACK_TEMPORARY_DIRECTORY}"
    /p "${APPX_PACKAGE_X64_FULL_PATH}"
    /o
    RESULT_VARIABLE _res_pack
    OUTPUT_VARIABLE _out_pack
    ERROR_VARIABLE _err_pack
)
message(STATUS "MSIX: Packaging 64-bit app (Input: ${CPACK_TEMPORARY_DIRECTORY}, Output: ${APPX_PACKAGE_X64_FULL_PATH})")
# Always print MakeAppx.exe pack output
message(STATUS "MSIX: MakeAppx.exe pack stdout:\n${_out_pack}")
if(_err_pack)
    message(WARNING "MSIX: MakeAppx.exe pack stderr:\n${_err_pack}")
endif()
if(NOT _res_pack EQUAL 0)
    message(FATAL_ERROR "MSIX: MakeAppx.exe pack failed with result: ${_res_pack}. See stdout/stderr above.")
endif()
# Verify .appx package was created (optional, but good practice)
if(NOT EXISTS "${APPX_PACKAGE_X64_FULL_PATH}")
    message(FATAL_ERROR "MSIX: CRITICAL - MakeAppx.exe pack appeared to succeed, but output file NOT FOUND: ${APPX_PACKAGE_X64_FULL_PATH}")
else()
    message(STATUS "MSIX: Package file ${APPX_PACKAGE_X64_FULL_PATH} successfully created.")
endif()

execute_process(
    COMMAND "${MAKEAPPX_EXE}"
    bundle
    /d "${APPX_PACKAGES_FULL_DIR}"
    /p "${APPX_BUNDLE_FULL_PATH}"
    /o
    RESULT_VARIABLE _res_bundle
    OUTPUT_VARIABLE _out_bundle
    ERROR_VARIABLE _err_bundle
)
message(STATUS "MSIX: Bundling all packages (Input: ${APPX_PACKAGES_FULL_DIR}, Output: ${APPX_BUNDLE_FULL_PATH})")
message(STATUS "MSIX: MakeAppx.exe bundle stdout:\n${_out_bundle}")
if(_err_bundle)
    message(WARNING "MSIX: MakeAppx.exe bundle stderr:\n${_err_bundle}")
endif()
if(NOT _res_bundle EQUAL 0)
    message(FATAL_ERROR "MSIX: MakeAppx.exe bundle failed with result: ${_res_bundle}. See stdout/stderr above.")
endif()

# NEW: Explicit check for the bundle file's existence
if(NOT EXISTS "${APPX_BUNDLE_FULL_PATH}")
    message(FATAL_ERROR "MSIX: CRITICAL - MakeAppx.exe bundle appeared to succeed (result: ${_res_bundle}), but the output bundle file was NOT FOUND at expected path: ${APPX_BUNDLE_FULL_PATH}. Check MakeAppx bundle stdout/stderr for details.")
else()
    message(STATUS "MSIX: Bundle file ${APPX_BUNDLE_FULL_PATH} successfully created and found.")
endif()

# Commands for .pdb to .appxsym conversion and .appxupload creation
message(STATUS "MSIX: Converting .pdb to .appxsym (PDB source: ${APPX_PDB_SOURCE_PATH}, Temp Symbol Zip: ${APPX_SYM_FULL_PATH}.zip) using cmake -E tar...")
# Re-check PDB existence right before compression
if(NOT EXISTS "${APPX_PDB_SOURCE_PATH}")
    message(FATAL_ERROR "MSIX: INTERNAL ERROR - PDB file ${APPX_PDB_SOURCE_PATH} was reported as existing by initial check, but is MISSING right before cmake -E tar (with symbols path).")
else()
    message(STATUS "MSIX: PDB file ${APPX_PDB_SOURCE_PATH} confirmed to still exist before cmake -E tar (with symbols path).")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cvf "${APPX_SYM_FULL_PATH}.zip" --format=zip "${APPX_PDB_SOURCE_PATH}"
    RESULT_VARIABLE _res_cmake_zip_pdb
    OUTPUT_VARIABLE _out_cmake_zip_pdb
    ERROR_VARIABLE _err_cmake_zip_pdb
)
message(STATUS "MSIX: cmake -E tar for PDB stdout:\n${_out_cmake_zip_pdb}")
if(_err_cmake_zip_pdb)
    message(WARNING "MSIX: cmake -E tar for PDB stderr:\n${_err_cmake_zip_pdb}")
endif()
if(NOT _res_cmake_zip_pdb EQUAL 0)
    message(FATAL_ERROR "MSIX: cmake -E tar for PDB failed with result: ${_res_cmake_zip_pdb}. See stdout/stderr above.")
endif()

if(NOT EXISTS "${APPX_SYM_FULL_PATH}.zip")
    message(FATAL_ERROR "MSIX: INTERNAL ERROR - Expected zip file ${APPX_SYM_FULL_PATH}.zip was NOT created by cmake -E tar for PDB.")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E rename "${APPX_SYM_FULL_PATH}.zip" "${APPX_SYM_FULL_PATH}"
    RESULT_VARIABLE _res_cmake_rename_sym
    OUTPUT_VARIABLE _out_cmake_rename_sym
    ERROR_VARIABLE _err_cmake_rename_sym
)
message(STATUS "MSIX: cmake -E rename for .appxsym stdout:\n${_out_cmake_rename_sym}")
if(_err_cmake_rename_sym)
    message(WARNING "MSIX: cmake -E rename for .appxsym stderr:\n${_err_cmake_rename_sym}")
endif()
if(NOT _res_cmake_rename_sym EQUAL 0)
    message(FATAL_ERROR "MSIX: cmake -E rename for .appxsym failed with result: ${_res_cmake_rename_sym}. See stdout/stderr above.")
endif()

message(STATUS "MSIX: Creating .appxupload (Bundle: ${APPX_BUNDLE_FULL_PATH}, Symbols: ${APPX_SYM_FULL_PATH}, Output: ${APPX_UPLOAD_FULL_PATH}) using cmake -E tar...")
if(NOT EXISTS "${APPX_BUNDLE_FULL_PATH}")
    message(FATAL_ERROR "MSIX: INTERNAL ERROR - Bundle file ${APPX_BUNDLE_FULL_PATH} is MISSING right before cmake -E tar (with symbols path).")
endif()
if(NOT EXISTS "${APPX_SYM_FULL_PATH}") # Check for the renamed .appxsym
    message(FATAL_ERROR "MSIX: INTERNAL ERROR - Symbol file ${APPX_SYM_FULL_PATH} is MISSING right before cmake -E tar (with symbols path).")
endif()
message(STATUS "MSIX: Bundle and Symbol files confirmed to exist before cmake -E tar (with symbols).")

# Find the VCLibs Desktop .appx package for x64
find_file(VCLIBS_DESKTOP_APPX_PATH
    NAMES "Microsoft.VCLibs.x64.14.00.Desktop.appx"
    HINTS
        "$ENV{ProgramFiles\(x86\)}/Microsoft SDKs/Windows Kits/10/ExtensionSDKs/Microsoft.VCLibs.Desktop/14.0/Appx/Retail/x64"
    REQUIRED
)
if(NOT VCLIBS_DESKTOP_APPX_PATH)
    message(FATAL_ERROR "MSIX: Could not find the VCLibs Desktop APPX package. Ensure the 'C++ Universal Windows Platform tools' are installed via the Visual Studio Installer.")
else()
    message(STATUS "MSIX: Found VCLibs Desktop APPX package at: ${VCLIBS_DESKTOP_APPX_PATH}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar cvf "${APPX_UPLOAD_FULL_PATH}.zip" --format=zip
            "${APPX_BUNDLE_FULL_PATH}"
            "${APPX_SYM_FULL_PATH}"
            "${VCLIBS_DESKTOP_APPX_PATH}"
    RESULT_VARIABLE _res_cmake_zip_with_sym
    OUTPUT_VARIABLE _out_cmake_zip_with_sym
    ERROR_VARIABLE _err_cmake_zip_with_sym
)
message(STATUS "MSIX: cmake -E tar for .appxupload (with symbols) stdout:\n${_out_cmake_zip_with_sym}")
if(_err_cmake_zip_with_sym)
    message(WARNING "MSIX: cmake -E tar for .appxupload (with symbols) stderr:\n${_err_cmake_zip_with_sym}")
endif()
if(NOT _res_cmake_zip_with_sym EQUAL 0)
    message(FATAL_ERROR "MSIX: cmake -E tar for .appxupload (with symbols) failed with result: ${_res_cmake_zip_with_sym}. See stdout/stderr above.")
endif()

if(NOT EXISTS "${APPX_UPLOAD_FULL_PATH}.zip")
    message(FATAL_ERROR "MSIX: INTERNAL ERROR - Expected zip file ${APPX_UPLOAD_FULL_PATH}.zip was NOT created by cmake -E tar for PDB.")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E rename "${APPX_UPLOAD_FULL_PATH}.zip" "${APPX_UPLOAD_FULL_PATH}"
    RESULT_VARIABLE _res_cmake_rename_sym
    OUTPUT_VARIABLE _out_cmake_rename_sym
    ERROR_VARIABLE _err_cmake_rename_sym
)
message(STATUS "MSIX: cmake -E rename for .appxsym stdout:\n${_out_cmake_rename_sym}")
if(_err_cmake_rename_sym)
    message(WARNING "MSIX: cmake -E rename for .appxsym stderr:\n${_err_cmake_rename_sym}")
endif()
if(NOT _res_cmake_rename_sym EQUAL 0)
    message(FATAL_ERROR "MSIX: cmake -E rename for .appxsym failed with result: ${_res_cmake_rename_sym}. See stdout/stderr above.")
endif()

message(STATUS "MSIX: Packaging complete. Final upload file: ${APPX_UPLOAD_FULL_PATH}")
# --- End of direct execution block ---

message(STATUS "MSIX: msix.cmake script configured for CPack External. Packaging will occur during CPack execution.")

set(CPACK_EXTERNAL_BUILT_PACKAGES "${APPX_UPLOAD_FULL_PATH}")
