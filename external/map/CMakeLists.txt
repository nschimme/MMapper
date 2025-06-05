# Handle map integration: either use a provided file or download from Git.
string(TOUPPER "${WITH_MAP}" WITH_MAP_UPPER)

if(WITH_MAP_UPPER STREQUAL "OFF" OR WITH_MAP_UPPER STREQUAL "FALSE")
    message(STATUS "Map integration disabled.")
else()
    set(MAP_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/map")
    file(MAKE_DIRECTORY "${MAP_DOWNLOAD_DIR}")

    # Define paths for the generated QRC file and the map XML in the build directory.
    set(MMAPPER_QRC_FILE "${MAP_DOWNLOAD_DIR}/map.qrc")
    set(MMAPPER_MAP_FILE_IN_BUILD "${MAP_DOWNLOAD_DIR}/arda")

    # Variable to hold the dependency for the map_copy target.
    # This will be either the copied file or the ExternalProject target.
    set(MAP_FILE_SOURCE_DEPENDENCY "")

    # --- Scenario: User provides an existing map file ---
    if(IS_ABSOLUTE "${WITH_MAP}" OR EXISTS "${WITH_MAP}")
        get_filename_component(PROVIDED_MAP_ABS "${WITH_MAP}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

        if(NOT EXISTS "${PROVIDED_MAP_ABS}")
            message(FATAL_ERROR "Provided map file or directory does not exist: ${PROVIDED_MAP_ABS}")
        endif()

        add_custom_command(
            OUTPUT "${MMAPPER_MAP_FILE_IN_BUILD}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROVIDED_MAP_ABS}" "${MMAPPER_MAP_FILE_IN_BUILD}"
            COMMENT "Copying provided map: ${PROVIDED_MAP_ABS}"
            VERBATIM
        )
        set(MAP_FILE_SOURCE_DEPENDENCY "${MMAPPER_MAP_FILE_IN_BUILD}")

    # --- Scenario: Download map from Git repository ---
    else()
        set(MMAPPER_MAP_SCHEMA "41") # Git tag for the map schema version.

        include(ExternalProject)
        ExternalProject_Add(map_repo
            GIT_REPOSITORY "https://github.com/MUME/arda"
            GIT_TAG "${MMAPPER_MAP_SCHEMA}"
            DOWNLOAD_DIR "${MAP_DOWNLOAD_DIR}/_download"
            SOURCE_DIR "${MAP_DOWNLOAD_DIR}/_source"
            CONFIGURE_COMMAND "" BUILD_COMMAND "" UPDATE_COMMAND "" # No build steps needed.
            INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/arda.xml "${MMAPPER_MAP_FILE_IN_BUILD}"
            BUILD_BYPRODUCTS "${MMAPPER_MAP_FILE_IN_BUILD}"
        )
        set(MAP_FILE_SOURCE_DEPENDENCY map_repo)
    endif()

    # Create a target to ensure the map XML file is in place (copied or downloaded).
    add_custom_target(map_copy ALL DEPENDS "${MAP_FILE_SOURCE_DEPENDENCY}")

    # --- Generate Qt Resource (QRC) file from map XML ---
    add_custom_command(
        OUTPUT "${MMAPPER_QRC_FILE}"
        COMMAND ${CMAKE_COMMAND} -DINPUT_DIR=${MAP_DOWNLOAD_DIR} -DOUTPUT_FILE=${MMAPPER_QRC_FILE} -P "${CMAKE_SOURCE_DIR}/cmake/GenerateQRC.cmake"
        DEPENDS "${MMAPPER_MAP_FILE_IN_BUILD}" # QRC generation depends on the XML map file.
        COMMENT "Generating map.qrc"
        WORKING_DIRECTORY "${MAP_DOWNLOAD_DIR}"
        VERBATIM
    )

    # --- Define the main 'map' target for the project ---
    # This target signifies that all map resources are ready.
    add_custom_target(map ALL DEPENDS "${MMAPPER_QRC_FILE}")

    # Ensure the main 'map' target implicitly triggers the map file acquisition.
    add_dependencies(map map_copy)

    message(STATUS "Map QRC file will be included: ${MMAPPER_QRC_FILE}")
endif()
