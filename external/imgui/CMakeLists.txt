find_path(IMGUI_HEADER imgui.h PATHS /app/include/imgui /usr/include/imgui)

if(IMGUI_HEADER)
    message(STATUS "Found imgui headers: ${IMGUI_HEADER}")
    set(IMGUI_INCLUDE_DIR "${IMGUI_HEADER}" PARENT_SCOPE)
    set(IMGUI_SOURCES
        "${IMGUI_HEADER}/imgui.cpp"
        "${IMGUI_HEADER}/imgui_draw.cpp"
        "${IMGUI_HEADER}/imgui_tables.cpp"
        "${IMGUI_HEADER}/imgui_widgets.cpp"
        "${IMGUI_HEADER}/backends/imgui_impl_opengl3.cpp"
        PARENT_SCOPE
    )
    set(IMGUI_FOUND true PARENT_SCOPE)
else()
    message(STATUS "ImGui NOT found; downloading from local source tree")
    include(FetchContent)

    set(IMGUI_VERSION v1.91.5)

    FetchContent_Declare(
        imgui
        URL "https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.tar.gz"
    )

    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()

    set(IMGUI_INCLUDE_DIR "${imgui_SOURCE_DIR}" PARENT_SCOPE)
    set(IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        PARENT_SCOPE
    )
endif()

# Define an interface target for MMapper to use
add_library(imgui_interface INTERFACE)
if(IMGUI_HEADER)
    target_include_directories(imgui_interface INTERFACE "${IMGUI_HEADER}" "${IMGUI_HEADER}/backends")
else()
    target_include_directories(imgui_interface INTERFACE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
endif()
add_library(imgui::imgui ALIAS imgui_interface)
