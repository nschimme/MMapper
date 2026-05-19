# AGENTS.md

## Architecture Documentation

For a high-level overview of the project's structure, data flow, and key components, please refer to [docs/architecture.md](docs/architecture.md). Reading this first will help you understand the codebase and plan your changes more effectively.

## Coding Guidelines

To maintain consistency and avoid common pitfalls in this codebase, please follow these guidelines:

1.  **Signal2 System:** Use the custom `Signal2` system (`src/global/Signal2.h`) for internal logic and performance-critical events. Use standard Qt signals primarily for UI components. Always use `Signal2Lifetime` for automatic disconnection.
2.  **String Encoding:** **Never** use `QString::toStdString()`. It can cause encoding issues. Instead, use `mmqt::toStdStringUtf8()` from `src/global/TextUtils.h`.
3.  **Atomic File Saving:** When saving files, follow the atomic pattern: `flush()` -> `io::fsync()` -> `file.close()` -> `io::rename()`. This prevents data corruption during crashes or power failures.
4.  **Memory Management:**
    *   Avoid declaring `QObject` as value members if they are parented to their owner (e.g., `m_member(this)`). This causes crashes during destruction. Use heap-allocated pointers instead.
    *   Rely on Qt's parent-child system for cleanup of UI components.
5.  **Shortest Path Performance:** Use `thread_utils::parallel_for_each_tl` for relaxation phases in shortest path algorithms; it handles both single-threaded and parallel execution efficiently.

## Mandatory Build Guidelines

1.  **Environment:** All builds **must** be performed on a **Linux** system.
2.  **Code Style:** All commits **must** strictly adhere to the project's `clang-format` configuration.

---

## Speed Optimization: ccache (Mandatory)

To enable incremental, cached builds and reduce compilation time, `ccache` is **required**.

1.  **Install:** Ensure `ccache` is installed on Linux.
    * *Example:* `sudo apt-get install -y ccache`
2.  **CMake Configuration:** The initial `cmake` command **must** use the Debug build type and include the ccache launcher flags:

    ```bash
    cmake -B build/ \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
          -DCMAKE_C_COMPILER_LAUNCHER=ccache \
          ... [Other flags from BUILD.md]
    ```

---

## Code Style Application (Mandatory)

We use `clang-format-18` via Docker to ensure style consistency. Use the following command to automatically format your code.

1. **Directory:** Navigate to the Git root directory.

2. **Execution:** Run this command to perform an in-place edit to all source files with the required style:

    ```bash
    docker run --rm --platform linux/amd64 \
         -v "$(pwd):$(pwd)" -w "$(pwd)" \
         ghcr.io/jidicula/clang-format:18 \
         -i --Werror --style=file \
         $(find src tests -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' -o -iname '*.hpp')
    ```
