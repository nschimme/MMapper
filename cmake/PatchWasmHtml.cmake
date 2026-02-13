if(NOT OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR not set")
endif()

set(HTML_INPUT "${OUTPUT_DIR}/mmapper.html")
set(HTML_OUTPUT "${OUTPUT_DIR}/index.html")

if(NOT EXISTS "${HTML_INPUT}")
    message(STATUS "mmapper.html not found in ${OUTPUT_DIR}, searching...")
    file(GLOB_RECURSE FOUND_HTML "${OUTPUT_DIR}/*.html")
    if(FOUND_HTML)
        list(GET FOUND_HTML 0 HTML_INPUT)
        message(STATUS "Found HTML file at: ${HTML_INPUT}")
    else()
        # Try one level up
        file(GLOB_RECURSE FOUND_HTML_UP "${OUTPUT_DIR}/../*.html")
        if(FOUND_HTML_UP)
            list(GET FOUND_HTML_UP 0 HTML_INPUT)
            message(STATUS "Found HTML file up: ${HTML_INPUT}")
        else()
            file(GLOB ALL_FILES "${OUTPUT_DIR}/*")
            message(STATUS "Files in ${OUTPUT_DIR}: ${ALL_FILES}")
            message(FATAL_ERROR "Could not find any HTML file to patch")
        endif()
    endif()
endif()

message(STATUS "Patching ${HTML_INPUT} -> ${HTML_OUTPUT}")
file(READ "${HTML_INPUT}" CONTENT)

# Look for <head> (case-insensitive) and inject the script tag
if(CONTENT MATCHES "(<[Hh][Ee][Aa][Dd]>)")
    string(REGEX REPLACE "(<[Hh][Ee][Aa][Dd]>)" "\\1\n<script src=\"./coi-serviceworker.js\"></script>" CONTENT "${CONTENT}")
    message(STATUS "Injected COI script")
else()
    message(WARNING "Could not find <head> tag in ${HTML_INPUT}, script not injected")
endif()

file(WRITE "${HTML_OUTPUT}" "${CONTENT}")
