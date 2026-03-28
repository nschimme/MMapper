# cmake/GenerateManifest.cmake
# Assumes INPUT_DIRS and OUTPUT_FILE are passed as -D variables
# INPUT_DIRS should be a semicolon-separated list of directories

if(NOT INPUT_DIRS)
    message(FATAL_ERROR "INPUT_DIRS not specified for GenerateManifest.cmake")
endif()
if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not specified for GenerateManifest.cmake")
endif()

set(MANIFEST_CONTENT "{\n")
set(FIRST_ENTRY TRUE)

foreach(DIR_INFO ${INPUT_DIRS})
    # DIR_INFO is "prefix:path"
    string(REPLACE ":" ";" DIR_PARTS "${DIR_INFO}")
    list(GET DIR_PARTS 0 PREFIX)
    list(GET DIR_PARTS 1 DIR)

    file(GLOB_RECURSE FILES RELATIVE "${DIR}" "${DIR}/*")

    foreach(FILE ${FILES})
        if(NOT FIRST_ENTRY)
            string(APPEND MANIFEST_CONTENT ",\n")
        endif()
        set(FIRST_ENTRY FALSE)

        # We want the key to be "prefix/file_path_without_extension"
        get_filename_component(FILE_EXT "${FILE}" LAST_EXT)
        get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
        get_filename_component(FILE_NAME "${FILE}" NAME_WE)

        if(FILE_DIR STREQUAL "")
            set(KEY "${PREFIX}/${FILE_NAME}")
        else()
            set(KEY "${PREFIX}/${FILE_DIR}/${FILE_NAME}")
        endif()

        string(APPEND MANIFEST_CONTENT "  \"${KEY}\": \"${PREFIX}/${FILE}\"")
    endforeach()
endforeach()

string(APPEND MANIFEST_CONTENT "\n}\n")

file(WRITE "${OUTPUT_FILE}" "${MANIFEST_CONTENT}")
