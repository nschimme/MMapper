# MMapper Ideas and Feature Requests

This document tracks feature ideas, architectural improvements, and enhancements for MMapper. Entries are ordered from smallest to largest size/difficulty.

---

### 1. Detailed Path Jump Penalties
* **Size:** Small
* **Category:** Path Machine
* **Description:** Add fine-grained penalty options in the path machine for "long jumps". Differentiate penalty weights between 2D jumps and 3D coordinate leaps (changing z-levels), as well as multi-step directional leaps (e.g., `e-e` vs. multi-directional jumps).

### 2. Path Machine Undo/Redo & Rollback
* **Size:** Medium
* **Category:** Path Machine
* **Description:** Support rolling back pathing states when the path machine takes an incorrect turn in ambiguous or random terrain. Players can undo past steps, hint the mapper to the correct room, and roll forward again without losing mapped progress or causing deep-space mapping errors.

### 3. Room Content Pathing Hints
* **Size:** Medium
* **Category:** Path Machine
* **Description:** Utilize room contents (e.g., stationary objects, trees, or non-mobile features) as additional probabilistic hints for the path machine. Because mobs move, content-based hints should decay quickly and carry lightweight influence when resolving identical room descriptions.

### 4. Adaptive Path Machine Parameter Tuning
* **Size:** Medium
* **Category:** Path Machine
* **Description:** While fixed path machine penalties are configurable via Preferences, the path machine could automatically adapt its weight parameters based on user hints and manual corrections. When a user forces or hints a path, parameters that favor the correct path increase while opposing weights decrease, adapting dynamically to repetitive or random terrain.

### 5. Visual Output Filtering & Fading Entity Tracking
* **Size:** Medium
* **Category:** UI / Integrated Client
* **Description:**
  * Provide configurable visual widgets/panels for categorizing MUD output (e.g., separate communication windows, room descriptions, or character-specific chats).
  * Record last-known locations of mobs or players on the map and display fading map indicators that decay over time.

### 6. Embedded Client Scripting Engine
* **Size:** Medium–Large
* **Category:** Integrated Client
* **Description:** While MMapper features a built-in MUD client terminal with hotkeys and aliases, integrating an embedded scripting engine (e.g., Lua or Python) would enable sophisticated client-side automation and scripting.

### 7. Comprehensive Mapper Scripting API
* **Size:** Medium–Large
* **Category:** Scripting / Parser
* **Description:** Expand the CLI and parser capabilities into a full programmatical C++/scripting API. Allow client scripts to programmatically create/delete rooms, modify parser rules, trigger parse events, and adjust mapping parameters dynamically.

### 8. High-Level 3D Canvas Rendering Library
* **Size:** Large
* **Category:** Architecture / Graphics
* **Description:** Abstract the low-level raw OpenGL drawing code in `MapCanvas` into a higher-level 3D scene graph or object library. This will make GUI rendering easier to maintain and extend across platforms (Desktop and WebAssembly).

### 9. Relational Database Map Storage
* **Size:** Large
* **Category:** Map Storage
* **Description:** Replace or supplement binary `.map` files with an extendable relational database engine (e.g., SQLite) paired with an in-memory C++ cache layer. This simplifies schema evolution, room indexing, and metadata extensions without complex binary file format migrations.

### 10. Sub-Areas / Embedded Nested Maps
* **Size:** Extra Large
* **Category:** Map Architecture
* **Description:** Support nested sub-areas (such as towns, castles, or cave systems) that represent as a single room node on the macro world map but open into dedicated detailed sub-maps upon inspection.
