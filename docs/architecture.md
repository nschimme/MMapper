# MMapper Architecture Overview

MMapper is a graphical mapping tool for the MUD game MUME. It acts as a bridge between the MUD server and the game client, providing an interactive map and enhanced features.

## 1. High-Level Data Flow (The Proxy Pipeline)

The `Proxy` class (`src/proxy/proxy.h`) manages the connection between the MUD server and the user's game client. It implements a pipeline for bidirectional data flow:

### From User to MUD
`UserSocket` -> `UserTelnet` -> `TelnetLineFilter` -> `AbstractParser` (UserInputParser) -> `MudTelnet` -> `MudSocket`

*   **UserSocket**: Raw TCP/WebSocket connection from the game client.
*   **UserTelnet**: Handles Telnet protocol (negotiation, IAC sequences).
*   **TelnetLineFilter**: Buffers raw bytes into lines.
*   **AbstractParser**: Processes user commands (e.g., `.map`, `.mark`). Commands not handled by MMapper are forwarded to `MudTelnet`.

### From MUD to User
`MudSocket` -> `MudTelnet` -> `TelnetLineFilter` -> `MpiFilter` -> {`RemoteEdit` or `MumeXmlParser` (MudParser)} -> `UserTelnet` -> `UserSocket`

*   **MudSocket**: Raw connection to MUME.
*   **MudTelnet**: Handles Telnet protocol and GMCP (Game Master Client Protocol).
*   **MpiFilter**: Historically used for the legacy MPI protocol; now handles out-of-band "MUME.Client" GMCP messages for remote editing or viewing.
*   **MumeXmlParser**: Parses game data (vitals, room info, exits) to update the map and internal state.

## 2. Key Components

### PathMachine (`src/pathmachine/`)
The core auto-mapping algorithm. It reconciles game events with the map database.
*   **States**:
    *   **APPROVED**: Confident of location; uses "matching tolerance" for room descriptions.
    *   **EXPERIMENTING**: Ambiguous location; tracks multiple hypothesis paths and prunes them based on "penalties" and "bonuses".
    *   **SYNCING**: Lost track; performs a global search of room descriptions to find a match.

### Map and World State (`src/map/`, `src/mapdata/`)
*   **MapData**: Primary owner of the map state, including the `World` graph and `MarkerList`.
*   **World**: Graph representation of rooms and exits. Supports multiple "layers" for pseudo-3D representation.
*   **SpatialDb**: R-Tree for fast spatial lookups.
*   **ShortestPath**: Parallel Delta-Stepping algorithm for navigation.
*   **MapHistory**: Manages undo/redo operations by tracking `Change` objects.

### Map Storage (`src/mapstorage/`)
*   **Formats**: Supports MMP (binary), JSON, XML, and Pandora.
*   **Load/Save**: Uses `MapSource` and `MapDestination` abstractions.
*   **Atomic Save**: Uses `FileSaver` to ensure data integrity via flush/sync/rename.

### Rendering Architecture (`src/display/`, `src/opengl/`)
MMapper uses a modern rendering pipeline focused on cross-platform compatibility (Desktop OpenGL & WebGL).
*   **MapCanvas**: A `QOpenGLWindow` that renders to an internal Framebuffer Object (FBO).
*   **FBO & Blitting**: Rendering is done to an FBO (optionally multisampled), which is then resolved and blitted to the default framebuffer via a full-screen triangle blit shader (`Functions::blitFboToDefault`).
*   **OpenGLProber**: Multi-process hardware survey (`mmapper-hardware-survey`) to select OpenGL 3.3, GLES 3.0, or WebGL 2.0 without crashing the main app.
*   **Unified Shaders**: Shaders (`src/resources/shaders/`) are written in a unified format for GL 3.3 and GLES 3.0/WebGL 2.0. The appropriate `#version` and precision headers are prepended at runtime via `Functions::getShaderVersion()`.
*   **Shared Buffers**: Uses Uniform Buffer Objects (UBOs) for efficient sharing of global state (colors, projection matrices) across shaders.

### Command & Syntax System (`src/syntax/`, `src/parser/`)
*   **AbstractParser**: The central dispatcher for user commands.
*   **Syntax Tree**: Uses a dedicated syntax parser (`src/syntax/`) to define and validate complex command structures.

### MUD Integration & World Tracking
*   **Remote Edit/View**: Special functionality handled by `MpiFilter` to open dedicated editor or viewer windows for content like maps or room descriptions. This uses the `MUME.Client` GMCP namespace.
*   **Group Management**: Tracks group member vitals and map positions via GMCP.
*   **Adventure Tracker**: Monitors XP gains, session duration, and character status.
*   **MumeClock**: Tracks Middle-earth time, moon phases, and seasons.
*   **GameObserver**: Centralized hub for world-state signals.

## 3. Core Design Patterns

### Signal2 System (`src/global/Signal2.h`)
A lightweight, high-performance alternative to Qt signals for core logic.
*   **Signal2Lifetime**: Required for safe disconnection.
*   **WeakHandle**: Prevents crashes when referencing objects across threads or lifetimes.

### Strategy Pattern
Used in `PathProcessor` (pathmachine), `MapStorage` (formats), and `Functions` (rendering backends).

### Pipeline Interfaces
The `Proxy` uses nested `Outputs` structs (e.g., `AbstractParserOutputs`) to decouple components and enforce explicit data flow.

## 4. Common Developer Tasks

### Adding a User Command
1.  Define the command syntax in `src/parser/AbstractParser.cpp`.
2.  Add a callback in `AbstractParser::initSpecialCommandMap`.
3.  Implement the logic in `AbstractParser`.

### Handling a New GMCP Message
1.  Add the message module to `src/proxy/GmcpModule.h`.
2.  Implement parsing in `MumeXmlParser::slot_parseGmcpInput` or `Mmapper2Group::slot_parseGmcpInput`.

### Creating a New Shader
1.  Add `.glsl` files to `src/resources/shaders/`.
2.  Declare the shader program struct in `src/opengl/legacy/Shaders.h`.
3.  Implement uniform setup and loading in `src/opengl/legacy/Shaders.cpp`.

## 5. Glossary

*   **GMCP**: Game Master Client Protocol. A JSON-based protocol over Telnet used for out-of-band data. See [MUME's GMCP documentation](https://mume.org/help/generic_mud_communication_protocol) for supported modules.
*   **MPI**: MUME Protocol Interface. A legacy protocol used for out-of-band content transmission; its functionality is largely superseded by the `MUME.Client` GMCP namespace, but the `MpiFilter` class name remains in the codebase.
*   **IAC**: Interpret As Command. Telnet escape character (`0xFF`).
*   **Vitals**: Character status data (HP, Mana, Moves).
*   **FBO**: Framebuffer Object. An off-screen rendering target.
*   **UBO**: Uniform Buffer Object. A buffer for sharing uniform data between shaders.
