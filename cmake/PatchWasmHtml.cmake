# Strip any literal quotes from the input directory path
string(REPLACE "\"" "" OUTPUT_DIR "${OUTPUT_DIR}")

set(HTML_INPUT "${OUTPUT_DIR}/mmapper.html")
set(HTML_OUTPUT "${OUTPUT_DIR}/index.html")

message(STATUS "-- PatchWasmHtml started")
message(STATUS "-- Checking for expected HTML at: ${HTML_INPUT}")

if(NOT EXISTS "${HTML_INPUT}")
    message(WARNING "-- ${HTML_INPUT} not found, searching in ${OUTPUT_DIR}...")
    file(GLOB_RECURSE FOUND_HTML "${OUTPUT_DIR}/mmapper.html")
    if(FOUND_HTML)
        list(GET FOUND_HTML 0 HTML_INPUT)
        message(STATUS "-- Found HTML at: ${HTML_INPUT}")
    else()
        message(FATAL_ERROR "-- Could not find mmapper.html in ${OUTPUT_DIR}")
    endif()
endif()

file(READ "${HTML_INPUT}" HTML_CONTENT)

# Inject COI script tag if not present
if(NOT HTML_CONTENT MATCHES "coi-serviceworker.js")
    string(REPLACE "<script src=\"qtloader.js\"></script>"
                   "<script src=\"coi-serviceworker.js\"></script>\n    <script src=\"qtloader.js\"></script>"
                   HTML_CONTENT "${HTML_CONTENT}")
    message(STATUS "-- Injected COI script tag")
else()
    message(STATUS "-- COI script tag already present")
endif()

message(STATUS "-- Patching ${HTML_INPUT} -> ${HTML_OUTPUT}")
file(WRITE "${HTML_OUTPUT}" "${HTML_CONTENT}")
message(STATUS "-- PatchWasmHtml finished successfully")
