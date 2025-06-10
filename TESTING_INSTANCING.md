# Testing Instanced Rendering

This document provides guidance on how to test the instanced rendering implementation in MMapper.

## Prerequisites

- A build of the MMapper application that includes the instancing changes.
- A large map file. For example, a map with ~28,000 rooms or more would be suitable for observing performance differences. If you have access to the "Realms of Despair" map data, that is a good candidate.

## Testing Steps

1.  **Run the Application**:
    Launch the MMapper application with the large map loaded.
    ```bash
    # Example:
    ./mmapper path/to/your/large_map.map
    ```

2.  **Observe Frame Time**:
    *   Open the console or log output of the application.
    *   Look for log messages related to performance statistics, specifically the "Frame time (actuallyPaintGL)" value. This value is logged in milliseconds (ms) at the end of each `paintGL` cycle if performance statistics are enabled.
    *   To enable performance statistics, you might need to check the application's settings or configuration. In MMapper, this is typically found under `Canvas -> Advanced -> Show performance statistics`. Ensure this is checked.

    Example log output to look for:
    ```
    ... (other log messages) ...
    0.1 (updateTextures) + 5.2 (updateBatches) + 10.3 (actuallyPaintGL) + 0.0 (glFinish) = 15.6 ms
    Frame time (actuallyPaintGL): 10.3 ms
    ... (other log messages) ...
    ```
    *   Move the map around, zoom in and out, and observe how the frame time changes. Note down typical or average frame times.

3.  **Visual Inspection**:
    *   Carefully navigate through different areas of the map.
    *   Pay attention to the rendering of rooms:
        *   **Positions**: Are rooms appearing in their correct locations?
        *   **Rotations/Scales**: If rooms have specific rotations or scaling (though less common for standard room geometry in MMapper), are these rendered correctly? The primary check is for position.
        *   **Overlapping/Z-fighting**: Check if rooms are correctly layered, especially when viewing different Z-levels or complex areas.
        *   **Missing Rooms**: Ensure all expected rooms are visible.
    *   The instancing changes primarily affect how room geometry is submitted to the GPU. While shaders were modified to accept instance matrices, the core geometry of a single room remains the same. The main risk with instancing is incorrect transformation matrices leading to misplaced rooms.

4.  **Performance Comparison (Optional but Recommended)**:
    *   If you have access to a previous version of MMapper *without* the instancing changes:
        1.  Run the previous version with the same large map and performance statistics enabled.
        2.  Record the typical "Frame time (actuallyPaintGL)" values.
        3.  Compare these values with the ones obtained from the version *with* instancing.
    *   Instancing is expected to improve performance (i.e., lower the frame time), especially on large maps where many similar objects (rooms) are rendered. The degree of improvement can vary based on the GPU, drivers, and the specific map.

5.  **Reporting**:
    *   If you encounter any visual rendering artifacts (e.g., rooms in wrong places, flickering, missing rooms), please take screenshots and note the map coordinates or area where the issue occurs.
    *   Report the observed frame times (e.g., "With instancing: ~X ms, Without instancing: ~Y ms").
    *   Include details about your system (OS, CPU, GPU, MMapper version/commit).
    *   Submit this information as an issue or report to the development team.

## What to Look For

*   **Performance Improvement**: The primary goal of instancing is to reduce CPU overhead and improve GPU efficiency, leading to lower frame times and smoother rendering on large maps.
*   **Rendering Correctness**: Ensure that the visual output is identical to a non-instanced version (or as expected if there were prior rendering bugs). Instancing should not introduce visual errors.

By following these steps, you can help verify the correctness and performance impact of the instanced rendering implementation.
