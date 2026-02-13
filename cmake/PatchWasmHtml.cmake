message(STATUS "PatchWasmHtml started")
if(NOT OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR not set")
endif()

message(STATUS "Searching for any HTML files in ${OUTPUT_DIR}/..")
file(GLOB_RECURSE ALL_HTML "${OUTPUT_DIR}/../*.html")
foreach(HTML_FILE ${ALL_HTML})
    message(STATUS "Found HTML: ${HTML_FILE}")
endforeach()

set(HTML_INPUT "${OUTPUT_DIR}/mmapper.html")
set(HTML_OUTPUT "${OUTPUT_DIR}/index.html")

if(NOT EXISTS "${HTML_INPUT}")
    message(STATUS "mmapper.html not found at expected path: ${HTML_INPUT}")
    if(ALL_HTML)
        list(GET ALL_HTML 0 HTML_INPUT)
        message(STATUS "Falling back to first found HTML: ${HTML_INPUT}")
    else()
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
