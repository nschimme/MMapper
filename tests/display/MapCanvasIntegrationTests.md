# MapCanvas and AsyncMapAreaManager Integration Tests

This document outlines key interaction points and expected behaviors for integration testing of `MapCanvas` with the `AsyncMapAreaManager`. These tests are intended to verify that the two components work together correctly in a more complete application environment.

## 1. Initialization

*   **Test Point**: `MapCanvas` constructor.
*   **Verification**:
    *   Ensure that a `display_async::AsyncMapAreaManager` instance (`m_asyncMapAreaManager`) is successfully constructed as a member of `MapCanvas` when `MapCanvas` is instantiated.
    *   Verify there are no crashes or errors during the construction of either class.

## 2. Handling Area Remesh Signals (`MapCanvas::slot_handleAreaRemesh`)

This slot is triggered by `MapData::needsAreaRemesh`.

*   **Test Point**: `MapCanvas::slot_handleAreaRemesh(const std::set<RoomArea>& areas)`.
*   **Verification (Specific Areas)**:
    *   Simulate `MapData` emitting `needsAreaRemesh` with a non-empty set of `RoomArea` objects.
    *   Verify that `slot_handleAreaRemesh` is called.
    *   For each `RoomArea` in the input set:
        *   Verify that `m_asyncMapAreaManager.requestAreaMesh()` is called.
        *   Verify that the `display_async::MapAreaRequestContext` passed to `requestAreaMesh` is correctly formed:
            *   `context.areaName` matches the area name from the `RoomArea` object.
            *   `context.mapSnapshot` is a valid `ConstMapHandle` obtained from `m_data.getCurrentMap()`.
            *   `context.textures` points to `m_textures`.
*   **Verification (Global Remesh)**:
    *   Simulate `MapData` emitting `needsAreaRemesh` with an empty set of `RoomArea` objects.
    *   Verify that `slot_handleAreaRemesh` is called.
    *   Verify that `MapCanvas` attempts to identify all relevant map areas (e.g., by calling `m_data.getCurrentMap()->getAreas()`).
    *   For each identified area:
        *   Verify that `m_asyncMapAreaManager.requestAreaMesh()` is called.
        *   Verify that the `display_async::MapAreaRequestContext` is correctly formed for each area.
*   **Verification (Map Not Loaded/No Areas)**:
    *   If a global remesh is requested but the map is not loaded or contains no areas, verify that `slot_handleAreaRemesh` handles this gracefully (e.g., logs a message, does not attempt to request meshes).
*   **Verification (OpenGL Not Initialized)**:
    *   If `m_opengl` is not yet initialized when the signal is received, verify that `slot_handleAreaRemesh` defers or skips processing gracefully (as per its current implementation).

## 3. Processing Completions

*   **Test Point**: Regular update path in `MapCanvas` (e.g., within `MapCanvas::actuallyPaintGL()` or a dedicated update tick if one exists).
*   **Verification**:
    *   Ensure that `m_asyncMapAreaManager.processCompletions(m_opengl, m_glFont)` is called regularly (e.g., once per frame or per update cycle).
    *   This can be verified by logging within `processCompletions` or by observing state changes in tasks managed by `AsyncMapAreaManager` over time.

## 4. Rendering Logic (`MapCanvas::renderMapBatches`)

*   **Test Point**: `MapCanvas::renderMapBatches()` (which is called by `MapCanvas::paintMap()`, which is called by `MapCanvas::actuallyPaintGL()`).
*   **Verification**:
    *   **Area Iteration**: Verify that `renderMapBatches` correctly identifies and iterates through the areas that should be rendered (e.g., all areas in the current map, or eventually, only visible areas if culling is implemented).
    *   **Fetching Batches**: For each relevant `areaName`:
        *   Verify that `m_asyncMapAreaManager.getCompletedBatch(areaName)` is called.
    *   **Conditional Rendering**:
        *   If `getCompletedBatch()` returns `nullptr` (no completed batch for an area), verify that no attempt is made to render that area's map geometry.
        *   If `getCompletedBatch()` returns a valid `const display_async::MapBatches*`:
            *   **Layer Meshes**: Verify that the code iterates through `area_map_batches->batchedMeshes` and calls `LayerMeshes::render()` for each layer.
            *   **Connection Meshes**: Verify that the code iterates through `area_map_batches->connectionMeshes` and calls `ConnectionMeshes::render()` for each.
            *   **Room Names**:
                *   Verify that the code accesses `area_map_batches->roomNameBatches` for the `current_map_layer`.
                *   Verify it iterates through the `std::vector<display_async::FontMesh3dInstance>`.
                *   For each `FontMesh3dInstance`, verify that OpenGL calls are made to:
                    *   Set up a transformation matrix based on `instance.worldPosition` (e.g., using `gl.translate()`).
                    *   Draw the `instance.mesh` (e.g., using `m_opengl.drawMesh()`).
    *   **Correct Parameters**: Ensure `LayerMeshes::render()` and `ConnectionMeshes::render()` are called with correct layer focus parameters (e.g., `current_map_layer`, `player_focus_layer`).

## 5. State Handling and Visual Feedback (e.g., in `MapCanvas::paintMap`)

*   **Test Point**: `MapCanvas::paintMap()` or other UI update logic.
*   **Verification**:
    *   **Loading Indication**:
        *   When map areas have been requested but their batches are not yet available (i.e., tasks are `PENDING_ASYNC` or `PENDING_FINISH`), verify that `MapCanvas` displays appropriate "loading" or "pending update" feedback.
        *   This involves checking if `MapCanvas` calls `m_asyncMapAreaManager.getTaskState(areaName)` for relevant areas.
        *   Verify that `setAnimating(true)` is called to ensure the "pending update" flash/message updates.
    *   **No Data/Failed State**:
        *   If an area's task has failed (`FAILED_ASYNC` or `FAILED_FINISH`), verify that it doesn't attempt to render data for that area and that any "loading" indication for that specific area stops or changes appropriately.
    *   **Completion**:
        *   When all relevant/visible area tasks are `COMPLETED` and there are no more pending operations, verify that `setAnimating(false)` is called (if no other animations are active).

This markdown document provides a checklist for verifying the correct integration and interaction between `MapCanvas` and `AsyncMapAreaManager`. These points can be tested manually in a running application or form the basis for automated integration tests if the testing environment allows for controlling `MapData` signals and observing `OpenGL` calls (e.g., through a spy/mock OpenGL implementation or by checking framebuffer output).
