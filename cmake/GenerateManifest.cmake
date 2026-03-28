# cmake/GenerateManifest.cmake
# Assumes INPUT_DIRS and OUTPUT_FILE are passed as -D variables
# INPUT_DIRS should be a semicolon-separated list of "prefix|path" pairs

if(NOT INPUT_DIRS)
    message(FATAL_ERROR "INPUT_DIRS not specified for GenerateManifest.cmake")
endif()
if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not specified for GenerateManifest.cmake")
endif()

set(MANIFEST_ENTRIES "")

foreach(DIR_INFO ${INPUT_DIRS})
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

file(WRITE "${OUTPUT_FILE}" "${MANIFEST_CONTENT}")
