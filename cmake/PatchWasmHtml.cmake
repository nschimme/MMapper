message(STATUS "PatchWasmHtml started")

# Strip any literal quotes from the input directory
string(REPLACE "\"" "" OUTPUT_DIR "${OUTPUT_DIR}")

if(NOT OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR not set")
endif()

set(HTML_INPUT "${OUTPUT_DIR}/mmapper.html")
set(HTML_OUTPUT "${OUTPUT_DIR}/index.html")

message(STATUS "Checking for expected HTML at: ${HTML_INPUT}")

if(NOT EXISTS "${HTML_INPUT}")
    message(STATUS "mmapper.html not found at expected path, searching recursively in ${OUTPUT_DIR}/..")
    file(GLOB_RECURSE ALL_HTML "${OUTPUT_DIR}/../*.html")

    if(ALL_HTML)
        foreach(HTML_FILE ${ALL_HTML})
            message(STATUS "Found HTML file: ${HTML_FILE}")
        endforeach()
        list(GET ALL_HTML 0 HTML_INPUT)
        message(STATUS "Falling back to: ${HTML_INPUT}")
    else()
        message(STATUS "Listing all files in ${OUTPUT_DIR} and its parent to help debug:")
        execute_process(COMMAND ls -R "${OUTPUT_DIR}/..")
        message(FATAL_ERROR "No HTML files found anywhere in build directory!")
    endif()
endif()

message(STATUS "Patching ${HTML_INPUT} -> ${HTML_OUTPUT}")
file(READ "${HTML_INPUT}" CONTENT)

# Look for <head> (case-insensitive) and inject the script tag
if(CONTENT MATCHES "(<[Hh][Ee][Aa][Dd]>)")
    string(REGEX REPLACE "(<[Hh][Ee][Aa][Dd]>)" "\\1\n<script src=\"./coi-serviceworker.js\"></script>" CONTENT "${CONTENT}")
    message(STATUS "Injected COI script tag")
else()
    message(WARNING "Could not find <head> tag in ${HTML_INPUT}, script not injected")
endif()

file(WRITE "${HTML_OUTPUT}" "${CONTENT}")
message(STATUS "PatchWasmHtml finished successfully")
