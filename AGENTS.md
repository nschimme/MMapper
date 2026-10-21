# AGENTS.md

## Orientation

For a high-level overview of the project's structure, data flow, and key components, please refer to [docs/architecture.md](docs/architecture.md). For detailed build instructions across all platforms, see [BUILD.md](BUILD.md).

## Coding Guidelines

To maintain consistency and avoid common pitfalls in this codebase, please follow these guidelines:

1.  **Signal2 System:** Use the custom `Signal2` system (`src/global/Signal2.h`) for internal logic and performance-critical events. Use standard Qt signals primarily for UI components. Always use `Signal2Lifetime` for automatic disconnection.
2.  **String Encoding:** **Never** use `QString::toStdString()`. It can cause encoding issues. Instead, use `mmqt::toStdStringUtf8()` from `src/global/TextUtils.h`.
3.  **Atomic File Saving:** When saving files, follow the atomic pattern: `flush()` -> `io::fsync()` -> `file.close()` -> `io::rename()`. This prevents data corruption.
4.  **Memory Management:**
    *   Avoid declaring `QObject` as value members if they are parented to their owner (e.g., `m_member(this)`). Use heap-allocated pointers instead.
    *   Rely on Qt's parent-child system for cleanup of UI components.
5.  **Shortest Path Performance:** Use `thread_utils::parallel_for_each_tl` for relaxation phases in shortest path algorithms; it handles both single-threaded and parallel execution efficiently.

## Development Tips

### Tracing Data Flow
The `Proxy` pipeline (`src/proxy/proxy.cpp`) is the best place to start when tracing how data moves between the MUD and the user. Use `grep` to follow specific GMCP messages or Telnet commands through the pipeline.

### Testing
We use a variety of unit tests located in the `tests/` directory.
*   To build and run all tests: `mkdir build && cd build && cmake .. && ninja && ctest`
*   To run a specific test suite: `./build/tests/TestProxy` (replace `TestProxy` with the desired test executable name).

## Mandatory Build Guidelines

1.  **Environment:** All builds **must** be performed on a **Linux** system.
2.  **Code Style:** All commits **must** strictly adhere to the project's `clang-format` configuration.

---

## Speed Optimization: ccache (Mandatory)

To enable incremental, cached builds and reduce compilation time, `ccache` is **required**.

1.  **Install:** Ensure `ccache` is installed on Linux.
2.  **CMake Configuration:** The initial `cmake` command **must** use the Debug build type and include the ccache launcher flags:
    ```bash
    cmake -B build/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache ...
    ```

---

## Code Style Application (Mandatory)

We use `clang-format-18` via Docker to ensure style consistency. Run this from the Git root:

```bash
docker run --rm --platform linux/amd64 -v "$(pwd):$(pwd)" -w "$(pwd)" ghcr.io/jidicula/clang-format:18 -i --Werror --style=file $(find src tests -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' -o -iname '*.hpp')
```
