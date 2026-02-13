if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not set")
endif()

message(STATUS "Downloading coi-serviceworker.js to ${OUTPUT_FILE}...")
file(DOWNLOAD "https://raw.githubusercontent.com/gzuidhof/coi-serviceworker/refs/heads/master/coi-serviceworker.js" "${OUTPUT_FILE}"
     SHOW_PROGRESS
     STATUS status)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Failed to download coi-serviceworker.js: ${status_string}")
endif()
