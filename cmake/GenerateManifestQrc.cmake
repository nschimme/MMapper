# cmake/GenerateManifestQrc.cmake
# Assumes INPUT_FILE and OUTPUT_FILE are passed as -D variables

if(NOT INPUT_FILE OR NOT OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE must be specified for GenerateManifestQrc.cmake")
endif()

get_filename_component(INPUT_NAME "${INPUT_FILE}" NAME)

set(QRC_CONTENT "<RCC>\n  <qresource prefix=\"/assets\">\n    <file alias=\"${INPUT_NAME}\">${INPUT_FILE}</file>\n  </qresource>\n</RCC>\n")

# Only write if different
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" OLD_CONTENT)
    if(NOT "${OLD_CONTENT}" STREQUAL "${QRC_CONTENT}")
        file(WRITE "${OUTPUT_FILE}" "${QRC_CONTENT}")
    endif()
else()
    file(WRITE "${OUTPUT_FILE}" "${QRC_CONTENT}")
endif()
