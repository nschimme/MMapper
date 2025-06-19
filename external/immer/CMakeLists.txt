find_path(IMMER_INCLUDE_DIR immer/immer.hpp PATHS /app/include)

# Check if immer was found and set up include directories
if(IMMER_INCLUDE_DIR)
    message(STATUS "Found immer: ${IMMER_INCLUDE_DIR}")
    set(IMMER_INCLUDE_DIR ${IMMER_INCLUDE_DIR} PARENT_SCOPE)
    set(IMMER_FOUND true PARENT_SCOPE)
else()
    message(STATUS "Could not find a local immer; downloading to local source tree")
    include(ExternalProject)
    ExternalProject_Add(immer
        URL "https://github.com/arximboldi/immer/archive/v0.8.1.tar.gz"
        URL_HASH SHA1=93395759707290996000867336797AF5BC5BD7A1

        SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/immer"
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        ALWAYS 0
        )
    ExternalProject_Get_Property(immer SOURCE_DIR)
    # The headers for immer are in a subdirectory called "immer" after unpacking.
    set(IMMER_INCLUDE_DIR ${SOURCE_DIR}/immer PARENT_SCOPE)
endif()
