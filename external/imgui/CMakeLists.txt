find_path(IMGUI_SEARCH_DIR imgui/imgui.h PATHS /app/include /usr/include)

if(IMGUI_SEARCH_DIR)
    message(STATUS "Found imgui headers in: ${IMGUI_SEARCH_DIR}")
    set(IMGUI_DIR "${IMGUI_SEARCH_DIR}/imgui")
    set(IMGUI_SOURCES
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
        PARENT_SCOPE
    )
    set(IMGUI_INCLUDE_DIR "${IMGUI_DIR}" PARENT_SCOPE)
    set(IMGUI_FOUND true PARENT_SCOPE)

    add_library(imgui_interface INTERFACE)
    target_include_directories(imgui_interface INTERFACE "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
    add_library(imgui::imgui ALIAS imgui_interface)
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

    add_library(imgui_interface INTERFACE)
    target_include_directories(imgui_interface INTERFACE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    add_library(imgui::imgui ALIAS imgui_interface)
endif()
