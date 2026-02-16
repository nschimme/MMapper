if(IMGUI_FOUND)
    message(STATUS "ImGui found")
else()
    message(STATUS "ImGui NOT found; downloading and building from local source tree")
    include(ExternalProject)

    set(IMGUI_VERSION v1.91.5)
    set(IMGUI_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/imgui-src")
    set(IMGUI_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/imgui-build")

    if(MSVC)
        set(IMGUI_LIBRARY_NAME imgui.lib)
    else()
        set(IMGUI_LIBRARY_NAME libimgui.a)
    endif()

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/imgui-CMakeLists.txt"
"cmake_minimum_required(VERSION 3.11)
project(imgui CXX)
set(CMAKE_CXX_STANDARD 17)
add_library(imgui STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC . ./backends)
# Let imgui_impl_opengl3.cpp use its own internal loader
# which is used when no other loader is defined and we are on a system with GL/GLES headers.
# Actually, the internal loader is the default if we don't define anything.
")

    ExternalProject_Add(imgui_repo
        URL "https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.tar.gz"
        SOURCE_DIR "${IMGUI_SRC_DIR}"
        BINARY_DIR "${IMGUI_BUILD_DIR}"
        PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/imgui-CMakeLists.txt" "${IMGUI_SRC_DIR}/CMakeLists.txt"
        INSTALL_COMMAND ""
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DCMAKE_CXX_STANDARD=17
                   -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
        UPDATE_COMMAND ""
        BUILD_BYPRODUCTS "${IMGUI_BUILD_DIR}/${IMGUI_LIBRARY_NAME}"
    )

    set(IMGUI_LIBRARIES "${IMGUI_BUILD_DIR}/${IMGUI_LIBRARY_NAME}" PARENT_SCOPE)
    set(IMGUI_INCLUDE_DIR "${IMGUI_SRC_DIR}" PARENT_SCOPE)
    add_custom_target(imgui DEPENDS imgui_repo)
endif()
