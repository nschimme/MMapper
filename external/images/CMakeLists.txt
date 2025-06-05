# Handle image resource integration: use a provided directory or download from Git.
string(TOUPPER "${WITH_IMAGES}" WITH_IMAGES_UPPER)

if(WITH_IMAGES_UPPER STREQUAL "OFF" OR WITH_IMAGES_UPPER STREQUAL "FALSE")
    message(STATUS "Image resources disabled.")
else()
    set(MMAPPER_IMAGES_SOURCE_DIR "${CMAKE_BINARY_DIR}/images")
    file(MAKE_DIRECTORY "${MMAPPER_IMAGES_SOURCE_DIR}")

    # Define path for the generated QRC file.
    set(MMAPPER_QRC_FILE "${MMAPPER_IMAGES_SOURCE_DIR}/images.qrc")

    # Variable to hold the target dependency for the QRC generation.
    set(QRC_GENERATION_TARGET_DEPENDENCY "")

    # --- Scenario: User provides an existing resources directory ---
    if(IS_ABSOLUTE "${WITH_IMAGES}" OR EXISTS "${WITH_IMAGES}")
        get_filename_component(PROVIDED_RESOURCES_ABS "${WITH_IMAGES}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

        if(NOT EXISTS "${PROVIDED_RESOURCES_ABS}")
            message(FATAL_ERROR "Provided resources directory does not exist: ${PROVIDED_RESOURCES_ABS}")
        endif()

        # Define a custom command to copy the directory.
        # This command is associated with the 'copy_local_resources' target.
        add_custom_command(
            OUTPUT "${MMAPPER_IMAGES_SOURCE_DIR}/CMakeLists.txt.copy_stamp" # Output a stamp file that depends on the source dir
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROVIDED_RESOURCES_ABS}" "${MMAPPER_IMAGES_SOURCE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E touch "${MMAPPER_IMAGES_SOURCE_DIR}/CMakeLists.txt.copy_stamp"
            DEPENDS ${PROVIDED_RESOURCES_ABS} # This line is critical, but only for changes *within* the source dir at configure time.
                                            # For robust changes (add/remove files), a re-configure is needed.
            COMMENT "Copying provided resources to ${MMAPPER_IMAGES_SOURCE_DIR}"
            VERBATIM
        )
        # Create a custom target that performs the copy.
        add_custom_target(copy_local_resources ALL
            DEPENDS "${MMAPPER_IMAGES_SOURCE_DIR}/CMakeLists.txt.copy_stamp"
        )
        set(QRC_GENERATION_TARGET_DEPENDENCY copy_local_resources)

    # --- Scenario: Download images from Git repository ---
    else()
        set(RESOURCES_GIT_REPOSITORY "https://github.com/MUME/images")
        set(MMAPPER_IMAGES_TAG "main") # Git tag for the image resources.

        include(ExternalProject)
        ExternalProject_Add(images_repo
            GIT_REPOSITORY "${RESOURCES_GIT_REPOSITORY}"
            GIT_TAG "${MMAPPER_IMAGES_TAG}"
            SOURCE_DIR "${MMAPPER_IMAGES_SOURCE_DIR}" # Clone directly into the target directory.
            CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "" UPDATE_COMMAND "" # No build steps.
        )
        set(QRC_GENERATION_TARGET_DEPENDENCY images_repo)
    endif()

    # --- Generate Qt Resource (QRC) file from images ---
    # This command runs after the resources are copied or cloned.
    add_custom_command(
        OUTPUT "${MMAPPER_QRC_FILE}"
        COMMAND ${CMAKE_COMMAND} -DINPUT_DIR=${MMAPPER_IMAGES_SOURCE_DIR} -DOUTPUT_FILE=${MMAPPER_QRC_FILE} -P "${CMAKE_SOURCE_DIR}/cmake/GenerateQRC.cmake"
        # The QRC generation depends on the target that ensures the images are present.
        DEPENDS "${QRC_GENERATION_TARGET_DEPENDENCY}"
        COMMENT "Generating images.qrc"
        WORKING_DIRECTORY "${MMAPPER_IMAGES_SOURCE_DIR}"
        VERBATIM
    )

    # --- Define the main 'images' target for the project ---
    # This target signifies that all image resources are ready.
    add_custom_target(images ALL DEPENDS "${MMAPPER_QRC_FILE}")

    # Ensure the main 'images' target implicitly triggers the resource acquisition target.
    add_dependencies(images ${QRC_GENERATION_TARGET_DEPENDENCY})

    message(STATUS "Images QRC file will be included: ${MMAPPER_QRC_FILE}")
endif()
