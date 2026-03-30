# cmake/GenerateManifest.cmake
# Assumes INPUT_DIRS and OUTPUT_FILE are passed as -D variables
# INPUT_DIRS should be a @-separated list of "prefix|path" pairs

if(NOT INPUT_DIRS)
    # If no inputs, create an empty manifest
    file(WRITE "${OUTPUT_FILE}" "{\n}\n")
    return()
endif()
if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not specified for GenerateManifest.cmake")
endif()

set(MANIFEST_ENTRIES "")

# Use @ as a separator because ; is the list separator in CMake and gets messy when passed via -D
string(REPLACE "@" ";" INPUT_DIRS_LIST "${INPUT_DIRS}")

foreach(DIR_INFO ${INPUT_DIRS_LIST})
    # DIR_INFO is "prefix|path"
    string(REPLACE "|" ";" DIR_PARTS "${DIR_INFO}")
    list(GET DIR_PARTS 0 PREFIX)
    list(GET DIR_PARTS 1 DIR)

    if(EXISTS "${DIR}")
        file(GLOB_RECURSE FILES RELATIVE "${DIR}" "${DIR}/*")

        foreach(FILE ${FILES})
            # Skip directories and the manifest itself if somehow included
            if(IS_DIRECTORY "${DIR}/${FILE}" OR FILE STREQUAL "manifest.json")
                continue()
            endif()

            # We want the key to be "prefix/file_path_without_extension"
            get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
            get_filename_component(FILE_NAME "${FILE}" NAME_WE)

            if(FILE_DIR STREQUAL "")
                set(KEY "${PREFIX}/${FILE_NAME}")
            else()
                set(KEY "${PREFIX}/${FILE_DIR}/${FILE_NAME}")
            endif()

            # Use JSON escape rules for paths
            string(REPLACE "\\" "/" KEY_ESC "${KEY}")
            string(REPLACE "\\" "/" FILE_ESC "${FILE}")

            list(APPEND MANIFEST_ENTRIES "  \"${KEY_ESC}\": \"${PREFIX}/${FILE_ESC}\"")
        endforeach()
    endif()
endforeach()

# Join entries with commas and newlines
if(MANIFEST_ENTRIES)
    list(JOIN MANIFEST_ENTRIES ",\n" MANIFEST_BODY)
else()
    set(MANIFEST_BODY "")
endif()

set(MANIFEST_CONTENT "{\n${MANIFEST_BODY}\n}\n")

# Only write if different to prevent unnecessary rebuilds
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" OLD_CONTENT)
    if(NOT "${OLD_CONTENT}" STREQUAL "${MANIFEST_CONTENT}")
        file(WRITE "${OUTPUT_FILE}" "${MANIFEST_CONTENT}")
    endif()
else()
    file(WRITE "${OUTPUT_FILE}" "${MANIFEST_CONTENT}")
endif()
