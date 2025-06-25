```markdown
# AGENTS.md - Critical Guidelines for AI Agents (Concise)

**Core Mandates - NON-NEGOTIABLE:**

1.  **Build Must Pass**: Your code **MUST** compile and pass tests with **Clang** in a **Debug configuration**.
    *   Local build command (Linux example using Ninja):
        ```bash
        mkdir build && cd build
        CXX=clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON ..
        ninja
        ctest -V
        ```
2.  **String Conversions**: **ALL** `QString` <-> `std::string` conversions **MUST** use `TextUtils` functions (e.g., `TextUtils::toQStringUtf8()`, `TextUtils::toStdStringLatin1()`). Direct use of `QString::toStdString/fromStdString` is forbidden and will break CI.
3.  **Code Style**: **ALL C++ code changes MUST be formatted using `clang-format`** with the project's `.clang-format` file before submission. (Column limit 100, 4 spaces for indent, pointer align right: `char *ptr;`).

**Key Project Info & Practices:**

*   **CMake Build System & Ninja (Recommended for Agents)**:
    *   Configure CMake with the **Ninja generator** (e.g., `cmake -G Ninja ...`). This aligns with CI practices.
    *   Use `ninja` (from `ninja-build` package) as the build tool for simplicity, as shown in examples.
    *   To add new `.cpp`/`.h` files, edit `src/CMakeLists.txt` or `tests/CMakeLists.txt` and re-run CMake.
*   **C++ Standard**: C++17.
*   **Dependencies**:
    *   **Qt5**: Core dependency (version 5.15.0+). Use minimally, primarily for signals/slots and when Qt APIs require Qt types. Prefer stdlib for general utilities and containers.
    *   External libs (`glm`, `immer`) are in `/external`.
*   **Testing**:
    *   Run tests via `ctest` in your build directory.
    *   CI uses ASan/UBSan; aim for clean runs.
*   **General Conventions**:
    *   Use RAII (`std::unique_ptr`, `std::shared_ptr`).
    # The line below was adjusted to integrate the Qt signals/slots note more smoothly with the stdlib preference.
    *   Prefer C++ stdlib types (e.g., `std::vector`, `std::string`) over Qt types. Qt (e.g., signals/slots, specific Qt classes for Qt APIs) should be used when necessary for its features or API integration.
*   **Commit Messages**: Short subject (max 50 chars), blank line, then detailed body if needed.

**Workflow:**

1.  Code.
2.  Format with `clang-format`.
3.  Build with Clang (Debug type) using Ninja (see example build command).
4.  Run `ctest`.
5.  If all pass, submit.

Adherence to these is critical for successful integration.
```
