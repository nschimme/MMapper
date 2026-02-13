if(NOT HTML_INPUT OR NOT HTML_OUTPUT)
    message(FATAL_ERROR "HTML_INPUT or HTML_OUTPUT not set")
endif()

if(NOT EXISTS "${HTML_INPUT}")
    message(FATAL_ERROR "Input HTML file not found: ${HTML_INPUT}")
endif()

file(READ "${HTML_INPUT}" CONTENT)

# Look for <head> (case-insensitive) and inject the script tag
if(CONTENT MATCHES "(<[Hh][Ee][Aa][Dd]>)")
    string(REGEX REPLACE "(<[Hh][Ee][Aa][Dd]>)" "\\1\n<script src=\"./coi-serviceworker.js\"></script>" CONTENT "${CONTENT}")
    message(STATUS "Injected COI script into ${HTML_OUTPUT}")
else()
    message(WARNING "Could not find <head> tag in ${HTML_INPUT}, script not injected")
endif()

file(WRITE "${HTML_OUTPUT}" "${CONTENT}")
