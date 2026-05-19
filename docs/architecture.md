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
*   **MpiFilter**: Detects special "MPI" sequences for remote editing or viewing.
*   **MumeXmlParser**: Parses game data (vitals, room info, exits) to update the map and internal state.

## 2. Key Components

### PathMachine (`src/pathmachine/`)
The core auto-mapping algorithm. It reconciles game events with the map database.
*   **States**:
    *   **APPROVED**: Confident of location; uses "matching tolerance" for room descriptions.
    *   **EXPERIMENTING**: Ambiguous location; tracks multiple hypothesis paths and prunes them based on "penalties" and "bonuses" (distance, new rooms, etc.).
    *   **SYNCING**: Lost track; performs a global search of room descriptions to find a match.

### Map and World State (`src/map/`, `src/mapdata/`)
*   **MapData**: Primary owner of the map state, including the `World` graph and `MarkerList`.
*   **World**: Graph representation of rooms and exits. Supports multiple "layers" for pseudo-3D representation.
*   **SpatialDb**: R-Tree for fast spatial lookups and collision detection.
*   **ShortestPath**: Parallel Delta-Stepping algorithm for navigation and pathfinding.
*   **MapHistory**: Manages undo/redo operations by tracking `Change` objects.

### Map Storage (`src/mapstorage/`)
*   **Formats**: Supports MMP (binary), JSON, XML, and Pandora.
*   **Load/Save**: Uses `MapSource` and `MapDestination` abstractions.
*   **Atomic Save**: Uses `FileSaver` to ensure data integrity via flush/sync/rename.

### Rendering (`src/display/`, `src/opengl/`)
*   **MapCanvas**: `QOpenGLWindow` rendering to an FBO, resolved for multisampling, and blitted to the default framebuffer.
*   **OpenGLProber**: Multi-process hardware survey (`mmapper-hardware-survey`) to select OpenGL 3.3, GLES 3.0, or WebGL 2.0.
*   **ThemeManager**: Manages application-wide styles, colors, and icon sets.

### Command & Syntax System (`src/syntax/`, `src/parser/`)
*   **AbstractParser**: The central dispatcher for user commands.
*   **Syntax Tree**: Uses a dedicated syntax parser (`src/syntax/`) to define and validate complex command structures.

### MUD Integration & World Tracking
*   **MPI (Remote Edit/View)**: Special protocol handled by `MpiFilter` to open dedicated editor or viewer windows.
*   **Group Management**: Tracks group member vitals and map positions via GMCP.
*   **Adventure Tracker**: Monitors XP gains, session duration, and character status.
*   **MumeClock**: Tracks game-specific time (Middle-earth time) and moon phases.
*   **GameObserver**: Centralized hub for world-state signals (time, weather, ticks).

## 3. Core Design Patterns

### Signal2 System (`src/global/Signal2.h`)
A lightweight, high-performance alternative to Qt signals for core logic.
*   **Signal2Lifetime**: Required for safe disconnection.
*   **WeakHandle**: Prevents crashes when referencing objects across threads or lifetimes.

### Strategy Pattern
Used in `PathProcessor` (pathmachine), `MapStorage` (formats), and `Functions` (rendering backends) to provide interchangeable implementations.

### Pipeline Interfaces
The `Proxy` uses nested `Outputs` structs (e.g., `AbstractParserOutputs`) to decouple components and enforce explicit data flow.

## 4. Common Developer Tasks

### Adding a User Command
1.  Define the command syntax in `src/parser/AbstractParser.cpp`.
2.  Add a callback in `AbstractParser::initSpecialCommandMap`.
3.  Implement the logic in a new or existing `AbstractParser` method.

### Handling a New GMCP Message
1.  Identify the module in `src/proxy/GmcpModule.h`.
2.  Add parsing logic to `MumeXmlParser::slot_parseGmcpInput` or `Mmapper2Group::slot_parseGmcpInput`.
3.  Emit a signal or update state via `GameObserver`.

### Adding a Map Change Type
1.  Define the change in `src/map/ChangeTypes.h`.
2.  Implement `apply` and `revert` logic in `Change.cpp`.
3.  Update the visitor in `AbstractChangeVisitor.h`.

## 5. Glossary

*   **GMCP**: Game Master Client Protocol. A JSON-based protocol over Telnet used for structured data (vitals, room info).
*   **MPI**: A custom protocol for "Remote Edit" and "Remote View" functionality.
*   **IAC**: Interpret As Command. The escape character (`0xFF`) in the Telnet protocol.
*   **Arda**: The name of the game world in MUME.
*   **Vitals**: Character status data (HP, Mana, Moves).
