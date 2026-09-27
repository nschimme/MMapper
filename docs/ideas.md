# MMapper Ideas and Feature Requests

This document tracks feature proposals and architectural enhancements for MMapper. Entries are ordered from smallest to largest size/difficulty. Each entry identifies the specific user problem being solved alongside proposed technical implementation details.

---

### 1. Detailed Path Jump Penalties
* **Size:** Small
* **Category:** Path Machine
* **User Problem Solved:** Players moving quickly across multi-tile gaps or vertical levels frequently experience path machine drift, where MMapper incorrectly leaps across z-levels or long distances instead of following the actual walking path.
* **Description:** Add fine-grained penalty settings in the path machine for "long jumps". Differentiate penalty weights between 2D horizontal jumps and 3D coordinate leaps (changing z-levels), as well as multi-step directional leaps (e.g., `e-e` vs. multi-directional jumps).
* **Implementation Details:** Extend `PathParameters` and `Configuration::PathMachineSettings` with separate penalty multipliers for 2D horizontal jumps vs 3D vertical z-level jumps. Apply these penalties in `PathProcessor::calculateProbability` when evaluating coordinate distances between candidate rooms.

### 2. CLI Command for Force Updating Room
* **Size:** Small
* **Category:** Parser / CLI
* **User Problem Solved:** Players who operate MMapper primarily via keyboard or terminal scripts must break flow and use mouse GUI menus to force-relocate their character position when the auto-mapper loses position sync.
* **Description:** Expose a dedicated CLI command/alias (e.g., `_force <roomid>`) to trigger a force-update of the player's position to a specific room without needing to navigate through GUI context menus.
* **Implementation Details:** Register a new command `cmdForce` in `AbstractParser::initSpecialCommandMap()` in `src/parser/AbstractParser-Commands.cpp` that parses a room ID or target, invokes `m_mapData->forceToRoom(targetRoomId)`, and updates player tracking state.

### 3. Enhanced Visual Door Indicators for Delayed/Pickable Doors
* **Size:** Small–Medium
* **Category:** Map Canvas / Rendering
* **User Problem Solved:** During high-stress Player Killing (PK) or fast escape maneuvers, players cannot discern at a glance which doors have delayed closing, pickable locks, or hidden status, leading to tactical errors and unexpected deaths.
* **Description:** Render doors on the map canvas with distinct colors or visual glyphs based on door attributes like delayed closing (`ExitFlagEnum::DELAY`), pickable locks (`ExitFlagEnum::PICK`), or hidden exits.
* **Implementation Details:** Extend exit rendering shaders/mesh generation in `src/display/mapcanvas_gl.cpp` or `src/map/ExitDirection.cpp`. Query `ExitHandle::getExitFlags()` for `DELAY`, `PICK`, or `LOCKED` flags and assign dedicated line colors or door node textures in `GLRenderState`.

### 4. Mark / Highlight Unvisited Rooms
* **Size:** Medium
* **Category:** Map Canvas
* **User Problem Solved:** Explorers mapping new regions or searching for missing rooms struggle to identify which rooms or exit paths on the canvas map have not yet been visited.
* **Description:** Provide a canvas overlay or room rendering mode that visually highlights unvisited rooms or rooms containing unexplored/unvisited exits.
* **Implementation Details:** Add a room visit tracking mechanism in `MapData` or query room history state. In `MapCanvas`, render an unvisited indicator (e.g., translucent glow or outline tint) using `ColoredLineMesh` or a fragment shader uniform when the "Highlight Unvisited" canvas toggle is active.

### 5. Map Annotation & Text Marker Customization
* **Size:** Medium
* **Category:** Map Canvas / Preferences
* **User Problem Solved:** Text annotations on the canvas map can be hard to read against custom room colors or high-DPI displays due to fixed font sizes, lack of contrast controls, and unconfigurable text backgrounds.
* **Description:** Allow user customization of fonts, text sizes, colors, alignment, and background opacity for text annotations and map markers on the 3D canvas.
* **Implementation Details:** Add `AnnotationSettings` to `Configuration::CanvasSettings` and expose UI controls in `GraphicsPage`. Update `TextMarker` rendering in `MapCanvas` to utilize the configured `GLFont` metrics, background padding, and palette settings.

### 6. Room Content Pathing Hints
* **Size:** Medium
* **Category:** Path Machine
* **User Problem Solved:** In areas where multiple adjacent rooms have identical room descriptions (e.g., forests, plains, or repetitive corridors), the path machine frequently misidentifies the player's true room location.
* **Description:** Utilize room contents (e.g., stationary objects, trees, or non-mobile features) as decaying probabilistic hints for the path machine to resolve ambiguous room descriptions.
* **Implementation Details:** Store recent room content observations in `PathProcessor`. Assign a low-weight probability bonus when room contents match historical observations, applying a time-decay factor (`decayFactor = exp(-lambda * timeElapsed)`) to account for mobile entities moving around.

### 7. Adaptive Path Machine Parameter Tuning
* **Size:** Medium
* **Category:** Path Machine
* **User Problem Solved:** Users must manually tweak path machine penalty numbers in Preferences when moving between different MUD terrains (e.g., repetitive mazes vs. linear highways), which is tedious and unintuitive.
* **Description:** While fixed path machine penalties are configurable via Preferences, the path machine could automatically adapt its weight parameters based on user hints and manual corrections. When a user forces or hints a path, weights favor the correct path while opposing weights decrease, adapting dynamically to repetitive or random terrain.
* **Implementation Details:** When `Mmapper2PathMachine::forceUpdate` or manual position corrections occur, execute a feedback adjustment step in `PathProcessor`: increment weights for parameters that favored the user-selected room and decrease weights for parameters that led to discarded candidate paths.

### 8. Visual Output Filtering & Fading Entity Tracking
* **Size:** Medium
* **Category:** Integrated Client / UI
* **User Problem Solved:** Game console output gets overwhelmed with chat spam obscuring room descriptions, and players lose track of where mobs or enemy players were last spotted on the map as time passes.
* **Description:** Provide configurable visual widgets/panels for categorizing MUD output (chat vs. descriptions) and render fading map indicators that track last-known mob or player locations.
* **Implementation Details:** Parse room mob/player broadcast messages in `MudTelnet` or `AbstractParser`. Store timestamped entity locations in `MapData`. Render fading icon overlays on room coordinates in `MapCanvas` where alpha transparency decays based on elapsed time (`alpha = clamp(1.0 - elapsed / maxAge, 0.0, 1.0)`).

### 9. Secret / Encrypted Room Notes
* **Size:** Medium
* **Category:** Map Storage / UI
* **User Problem Solved:** Players storing sensitive or tactical map notes (such as secret vault combinations, hidden door passwords, or PK ambush spots) risk leaking private data when sharing map files with team members.
* **Description:** Support password-protected or encrypted private notes attached to map rooms for sensitive player information.
* **Implementation Details:** Store encrypted note byte arrays in `RoomNote` using AES-256 (via OpenSSL or Qt Cryptographic Framework). Prompt for a user password on room inspection or when editing encrypted notes in `MainWindow`.

### 10. Artificial Light State Tracking
* **Size:** Medium
* **Category:** Map Data / Parser
* **User Problem Solved:** Players entering dark rooms without tracking torch light states get confused about whether a room is naturally dark or missing illumination, leading to accidental movement into dangerous unlit areas.
* **Description:** Track artificial light sources (lit/dark states) on rooms, characters, or MUD environmental conditions to assist in managing dark areas and light equipment.
* **Implementation Details:** Add a `LightStateEnum` property to `Room` (`src/map/room.h`). Parse light state messages (e.g., torch lit/extinguished, day/night transitions) in `MumeXmlParser` and update room/player light status in `MapData`. Render light indicators on dark rooms in `MapCanvas`.

### 11. Zone & Area Boundaries Visualization
* **Size:** Medium
* **Category:** Map Canvas
* **User Problem Solved:** Navigating dense world maps makes it difficult to tell where one zone ends and another begins, making area-based navigation and zone boundary awareness unclear.
* **Description:** Render visual boundary outlines or shaded hulls separating different map areas or zones on the 3D canvas map view.
* **Implementation Details:** Compute 2D/3D convex or concave hulls around rooms belonging to the same `AreaId` in `WorldAreaMap`. Generate boundary contour lines or polygon meshes in `src/display/mapcanvas_gl.cpp` rendered with semi-transparent area-specific colors.

### 12. Extended Room Metadata Storage
* **Size:** Medium–Large
* **Category:** Map Storage / Parser
* **User Problem Solved:** Useful MUD world context (such as exit descriptions, room ambiance, or slope direction) is discarded when saving maps, preventing users from searching or inspecting detailed room lore.
* **Description:** Store extended MUD room properties including ambiance comments, exit descriptions, keywords, and sloping messages in the map database for richer room querying and display.
* **Implementation Details:** Extend `Room` data members (`src/map/room.h`) and `RoomFieldEnum` variants with `exitDescriptions`, `ambianceComments`, and `slopeFlags`. Update `jsonmapstorage.cpp` and `filesaver.cpp` serialization logic to save and load these properties in a backward-compatible manner.

### 13. In-game Calendar & Quest/Season Tracker
* **Size:** Medium–Large
* **Category:** UI / MUD Integration
* **User Problem Solved:** Players miss time-sensitive MUD quests, seasonal shop openings, or weather shifts because in-game Middle-earth date and time information requires manual polling commands.
* **Description:** Integrated tracking for MUME calendar dates, seasonal changes, weather conditions, and quest timelines in a dedicated UI panel.
* **Implementation Details:** Parse `MUME.Clock` GMCP messages and MUD time output in `MumeClock` (`src/clock/mumeclock.h`). Build a dedicated `CalendarWidget` dockable panel displaying current Middle-earth date, month, season, moon phase, and active quest timers.

### 14. Mini-map / Map-in-Map Overlay View
* **Size:** Medium–Large
* **Category:** Map Canvas / UI
* **User Problem Solved:** Zooming in close to inspect local room detail loses macro-level spatial context, forcing players to constantly zoom in and out to maintain regional orientation.
* **Description:** An inset overview mini-map widget positioned in the corner of the main map canvas providing a regional macro-level view without losing local detail.
* **Implementation Details:** Create a secondary offscreen FBO in `MapCanvas` with an expanded orthogonal projection matrix centered on player position. Render a simplified low-LOD map pass into this FBO and blit it as a viewport quad overlay in `MapCanvas::paintGL()`.

### 15. Maze Rendering & Mapping Tools
* **Size:** Medium–Large
* **Category:** Path Machine / Canvas
* **User Problem Solved:** Mapping MUD mazes with repeating loop connections or identical room descriptions distorts the 3D map canvas and breaks standard auto-mapping, creating tangled spatial room geometry.
* **Description:** Specialized visual modes and auto-mapping aids for navigating and rendering complex MUD mazes with repeating room descriptions.
* **Implementation Details:** Introduce a "Maze Mode" state in `Mmapper2PathMachine` that relaxes unique room coordinate constraints. Implement graph cycle detection for maze clusters and render specialized maze visual connections (e.g., curved or directional loop arrows) in `MapCanvas`.

### 16. Embedded Client Scripting Engine
* **Size:** Medium–Large
* **Category:** Integrated Client
* **User Problem Solved:** Power users who want complex automation, dynamic response macros, or automated map queries are restricted by the basic CLI alias/action system.
* **Description:** While MMapper features a built-in MUD client terminal with hotkeys and aliases, integrating an embedded scripting engine (e.g., Lua or Python) would enable sophisticated client-side automation and scripting.
* **Implementation Details:** Embed Lua (via `sol2` or C Lua API) into `src/client/`. Bind `ClientWidget` send methods, text line triggers, hotkey events, and map query functions to Lua state globals, allowing users to run custom `.lua` scripts.

### 17. Comprehensive Mapper Scripting API
* **Size:** Medium–Large
* **Category:** Scripting / Parser
* **User Problem Solved:** External client scripts and MUD plugins cannot programmatically inspect or modify mapper state, create rooms, or adjust mapping rules on the fly.
* **Description:** Expand the CLI and parser capabilities into a full programmatical C++/scripting API. Allow client scripts to programmatically create/delete rooms, modify parser rules, trigger parse events, and adjust mapping parameters dynamically.
* **Implementation Details:** Expose a unified C++ script interface (`MapperScriptAPI`) connecting `MapData`, `AbstractParser`, and `ScriptEngine`. Provide bindings for room creation, exit linking, event injection, and preference modification accessible via client scripts or CLI scripts.

### 18. High-Level 3D Canvas Rendering Library
* **Size:** Large
* **Category:** Architecture / Graphics
* **User Problem Solved:** The low-level raw OpenGL rendering codebase makes adding new canvas UI features or maintaining cross-platform compatibility (Desktop vs. WebAssembly) fragile and labor-intensive for developers.
* **Description:** Abstract the low-level raw OpenGL drawing code in `MapCanvas` into a higher-level 3D scene graph or object library. This will make GUI rendering easier to maintain and extend across platforms (Desktop and WebAssembly).
* **Implementation Details:** Replace raw shader calls and manual vertex buffer manipulation in `src/display/mapcanvas_gl.cpp` with a scene graph abstraction (`SceneNode`, `MeshNode`, `CameraNode`, `RenderPass`). Decouple canvas rendering from platform-specific OpenGL calls to facilitate WebAssembly and WebGL optimizations.

### 19. Relational Database Map Storage
* **Size:** Large
* **Category:** Map Storage
* **User Problem Solved:** Storing large map datasets in custom binary files makes data migration, schema updates, and instant text indexing difficult and prone to file corruption.
* **Description:** Replace or supplement binary `.map` files with an extendable relational database engine (e.g., SQLite) paired with an in-memory C++ cache layer. This simplifies schema evolution, room indexing, and metadata extensions without complex binary file format migrations.
* **Implementation Details:** Implement an `SqliteMapStorage` backend implementing the `MapStorage` interface. Design relational tables for `rooms`, `exits`, `notes`, `areas`, and `text_markers`. Build an LRU memory cache layer to ensure zero performance degradation during high-speed pathfinding or map rendering.

### 20. Sub-Areas / Embedded Nested Maps
* **Size:** Extra Large
* **Category:** Map Architecture
* **User Problem Solved:** Dense towns, castles, or underground dungeons clutter the macro world map canvas and distort room spacing in surrounding wilderness zones.
* **Description:** Support nested sub-areas (such as towns, castles, or cave systems) that represent as a single room node on the macro world map but open into dedicated detailed sub-maps upon inspection.
* **Implementation Details:** Refactor `World` and `Coordinate` structures to support hierarchical spatial spaces. Introduce `SubAreaRoom` nodes containing isolated internal coordinate systems. Update pathfinding (`shortestPathSearch`), canvas navigation, and room selection tools to navigate seamlessly across macro-map and sub-area coordinate layers.
