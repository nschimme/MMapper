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

### Map and World State (`src/map/`, `src/mapdata/`)
*   **MapData**: The primary owner of the map state.
*   **World**: Represents the game world (rooms, exits, areas).
*   **SpatialDb**: An R-Tree based database for fast spatial queries (e.g., "what room is at these coordinates?").

### Rendering (`src/display/`, `src/opengl/`)
*   **MapCanvas**: A `QOpenGLWindow` that performs the actual rendering using an internal FBO.
*   **OpenGLProber**: Detects hardware capabilities to select the best rendering backend (OpenGL Core 3.3, GLES 3.0, or WebGL 2.0).
*   **Functions**: A wrapper around OpenGL calls to provide cross-backend compatibility.

### Observer Pattern (`src/observer/`)
*   **GameObserver**: A centralized hub for world state changes (time, weather, ticks). Components connect to its `Signal2` signals to react to game events.

### Group Management (`src/group/`)
*   **Mmapper2Group**: Manages group members, their vitals, and their positions on the map.

## 3. Core Design Patterns

### Signal2 System (`src/global/Signal2.h`)
The project uses a custom, lightweight `Signal2` system instead of standard Qt signals in many performance-critical or non-`QObject` contexts.
*   **Signal2Lifetime**: Used to automatically disconnect signals when an object is destroyed.
*   **WeakHandle**: Provides a safe way to reference objects that might be destroyed.

### Pipeline Interfaces
Many components define their interactions through nested `Outputs` structs (e.g., `AbstractParserOutputs`, `MudTelnetOutputs`). This decouples the components and makes the data flow explicit in the `Proxy`.
