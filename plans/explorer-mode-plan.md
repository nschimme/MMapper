# Explorer Mode: Plan (v2.3, advisor-reviewed twice, decisions settled)

## 1. Goal

Ship a full MUME map, but let the player see only what their character has discovered. Secret
exits stay walls until found, and what is only reachable through one stays hidden until then.
Rooms that are not hidden (see "Visibility default" below) are shown without being discovered.
Doors animate when they open and close (in every mode).

Non-goals:
- Not anti-cheat. The full map still ships in the file; this is spoiler avoidance.
- Mappers keep full visibility in Map mode.
- No change to the map file format.

## 2. Decisions

Settled:

| Topic | Decision |
|---|---|
| Base | Use MUME/MMapper PR #566 for the GMCP `Room.Known` data layer. Do not use its red-dot overlay as the product. |
| Mode | New `MapModeEnum::EXPLORE` beside `PLAY`, `MAP`, `OFFLINE`. |
| Hidden exits | Closed (a plain wall) until discovered. |
| Server data wins | A live server observation always overrides cached or transient data. |
| Room.Known negotiation | `Room.Known 1` goes in the first `Core.Supports.Set`, before login, when Explore is the configured mode at connect (as the #566 branch did for everyone, which is the only sequence known to work against mume.org). Outside Explore it is not negotiated. Switching to Explore mid-session adds it on demand; an `Add` that changes nothing is not repeated as a `Set`. |
| Fail direction | Fail closed: anything whose status is unknown stays fogged. |
| Visibility default | **Changed (v2.3).** A room that is not hidden is visible without being discovered: reachable from the base map's seed rooms (the rooms "Export Base Map" keeps) through exits that are not secret, or whose secret the character has found. Only what is behind an undiscovered secret stays fogged, plus anything the map cannot reach from a seed. Known rooms (`Room.Known`) and the current room stay visible as before. The walk is the same code as the export (`World::findBaseMapRooms`), with the discovered exits as the one difference. If the map holds none of the seed rooms, Explore falls back to the old rule (known rooms only) and logs it once. |
| Door reset | Every 60 minutes of **real time**; transient door state expires on that interval (monotonic clock, injectable for tests). |
| Name | "Explore" in menus and status, matching "Play" and "Map". |
| Offline mode | Stays a "show everything" mode. Switching to it from Explore warns "This reveals the full map." "Disconnected Explore" means using Explore while not connected, from the cached profile. |
| Rooms with no server ID | Visible when not hidden (reachable from a seed through non-secret exits, see above). Otherwise fogged, unless the path machine has placed the player there this session. They have no stable key, so they cannot be persisted and fog again next session. |
| Group beacons on fogged rooms | The beacon is shown with the room drawn as a bare marker. Your own position is always shown. |
| #566 | Coordinate with its author, rebase onto master, and land the data layer first (the extension path, not a fork). |

**Why the default changed (v2.3), settled.** The fog of war hides true secrets only. The base
map is not a secret (it is what "Export Base Map" publishes), so it is always visible, with or
without the `Room.Known` stream. Fog covers what is behind an undiscovered secret and anything
the base map cannot reach from a seed. Fog levels dim them (section 3.4); they never hide them.
Without this rule a character whose `Room.Known` list never arrives saw an empty map.

**Frontier, decided:** glimpsed and visited are the same thing. `Room.Known` already includes
rooms seen through an exit, so it is the "visited" set, and frontier is computed from all known
rooms, which is what the policy does. No separate "entered" tracking is needed.

## 3. UX specification

### 3.1 Vocabulary (menus, status bar, settings, docs)

- **Explore mode**: the mode. Menu "Explore", status "Explore mode".
- **Fog**: the effect hiding true secrets (see "Visibility default"). UI label "Fog".
- **Discovered**: a room your character knows (entered or seen through an exit) or a secret
  exit you found. Never "visited".
- **Frontier**: an undiscovered room one step beyond a discovered one, reached only through
  exits the player can actually see (never through an undiscovered secret exit). A room
  glimpsed through an exit counts as discovered, the same as one entered.

### 3.2 Entering and leaving

- Mapper menu gets "Explore" beside Play, Map, Offline. The parser gets `_mode explore`. The
  mode persists in config (`isValidMapMode` must accept it).
- Leaving Explore for Play, Map, or Offline asks for confirmation: "This will show the full
  map." (one click would otherwise be a silent spoiler). A "Don't ask again" box is allowed.
- **Explore is read-only (decided and implemented).** The map refuses every edit while Explore
  is the mode (`MapFrontend::setReadOnly()`: `applyChanges`, undo and redo), whatever the UI
  does. The edit tools are disabled with the tooltip "Switch to Map mode to edit". Tension,
  listed under risks: fixing a stale map needs Map mode, which reveals everything.
- The one exception is the path machine's backfill of server IDs
  (`room_change_types::SetServerId`; `onlyServerIdBackfill()` drops everything else, including
  `HIDDEN` door flags, `NO_MATCH` and room updates). It is applied with
  `MapFrontend::applyBackfill()`: not undoable, and when the map was unmodified the saved map is
  re-baselined to the result before anyone is told, so there is no modified title, Save stays
  disabled and nothing prompts "save changes?". The cost is that backfilled IDs reach the disk
  only through a later explicit save; otherwise the next session fills them in again. A map
  that was already modified stays modified, and its saved map is not touched.

### 3.3 States

| State | Behaviour |
|---|---|
| Connected, list loaded | Normal Explore view. |
| Connected, list loading, cache present | Use the cache; "Updating discoveries..." status item. |
| Connected, list loading, no cache | Current room visible; rooms appear (without animation) as list chunks arrive; the rest stay fogged. Status item explains and shows progress. |
| Server does not offer `Room.Known` | **Stay in Explore**: current room plus any cache, with a persistent notice. Never fall back to a mode that reveals the map. |
| Disconnected | Cached profile if any; otherwise an empty-map hint "Connect to begin exploring". |
| Character not yet identified | Current room only until GMCP identifies the character. |
| Room with no server ID | Fogged (see section 2). |
| Current room | Always visible. |

### 3.4 Fog strength (one control)

The fog levels apply to the **undiscovered base map**: rooms that are not hidden (see
"Visibility default") but that the character has not discovered. They are dimmed, never hidden:
no level removes a base-map room. True secrets are hidden at every level, and the current room
is always clear.

Preferences > Graphics, beside the weather sliders: a preset dropdown **Off / Subtle / Medium /
Severe**. **Off is the default** until the shader draws the levels and someone has looked at
them on a real map: the undiscovered base map is then fully clear and only secrets are hidden.
The mask stores the level as the texel of an undiscovered base room (`ExplorerFogMask::
UNDISCOVERED`; Off writes `SHOWN`), and the room-cell shader fogs every texel below `SHOWN`
today, so the shader must learn that value before a level other than Off is selectable.
Frontier hints are obsolete (see "Decided" below), so there is no switch for them.

Layer matrix (what an **undiscovered base-map** room shows). The base map is not a secret, so
no layer of such a room is ever hidden: every layer dims with the level.

| Layer | Off | Subtle | Medium | Severe |
|---|---|---|---|---|
| Terrain | clear | dimmed, desaturated | parchment tint, softened edge | strongly dimmed, never dissolved |
| Mob and load overlays (herbs, treasure...) | clear | dimmed | dimmed | strongly dimmed |
| Infomarks and labels | clear | dimmed | dimmed | strongly dimmed |
| Exits, walls, doors | clear | dimmed | dimmed | strongly dimmed |
| Connections from a discovered room toward an undiscovered one | full line | full line | full line | full line |
| Room and door names | clear | dimmed | dimmed | strongly dimmed |

Hidden at every level, whatever the preset: rooms behind an undiscovered secret, undiscovered
secret exits and their door names (`hideUndiscoveredExits` already does this), and infomarks
and labels that sit over a hidden cell (infomarks are positional, so they need the mask in
the infomark and font shaders, not a remesh).

**Decided (v2.3):** every layer dims, none is hidden, so the map never contradicts Find Rooms,
which reports the whole base map. Default level: **Off** until someone has looked at the shader
on a real map, **Subtle** afterwards (a public map only needs a light "you have not been here"
cue). **Frontier hints are obsolete**: with the base map visible and found secrets part of the
walk, the frontier is nearly always empty. The code stays until Phase 5 is dropped.

**Requirement for the shader slice.** When the player leaves a cell whose base value is
`UNDISCOVERED`, patch it to `SHOWN`, not back to the base value: the player has stood there, so
it is discovered, and the mask only catches up when the `Room.Known` update arrives. Every
other value is restored from the base as now.

**Existing issue, outside this work.** "Export Base Map" removes rooms and exits but keeps every
infomark, and infomarks are positional. A mark over a room the export removes would still be
published. Whether any such mark exists on the shipped map has not been checked.

### 3.5 Motion

- **Discoveries:** a room fades in over about 400 ms when a `Room.Known.Add` arrives *after* the
  list transfer has finished and the room is on screen. This covers the rooms revealed through
  exits when you step somewhere new. Adds that arrive during the transfer, and cache loads,
  appear with no animation, so a minute-long first load does not cause thousands of fades.
- **Doors:** a door swings about 250 ms when a live observation changes a fresh state while you
  are in the room. State learned on entry, after a stale gap, or first seen, snaps.
- **Secret exits found:** the door animation plays once.
- **"Animate doors" is a general graphics setting that applies in every mode**, not part of the
  Fog group. "Animate discoveries" lives in the Fog group. A "Reduce motion" option disables
  all animations.

### 3.6 Consistency across the app

- Mapping Hints dots (missing ID, unsaved changes) are suppressed in Explore.
- Names, door names, infomarks, tooltips, the room panel, Find Rooms, and path commands use the
  same visibility policy, so the map never contradicts itself.
- **Path finding and the path overlay only use discovered rooms and exits.** The overlay is not
  a spoiler only because of this restriction.
- **Group beacons:** a member standing on a fogged room would reveal that it exists. See the
  settled decision in section 2.

## 4. Architecture

### 4.1 Data

| Data | Lifetime | Home | Persisted |
|---|---|---|---|
| Door open/closed/broken | 60 min TTL | `DoorTracker` (session; phase 4) | no |
| Discovered secret exits | permanent, per character | `KnownRoomsTracker` plus `DiscoveredExitsStore` (`.exits` file) | yes |
| Known rooms | permanent, per character | `KnownRoomsTracker` plus `KnownRoomsStore` (`.bin` file) | yes |
| Fog mask, door-state texture | derived | renderer | no |

- Trackers follow the repo pattern (`AdventureTracker`, `MumeClock`): created in `MainWindow`
  with a `GameObserver&`, subscribed to GMCP signals, exposing state and a change signal.
  `GameObserver` stays a thin signal hub (unlike #566, which put `KnownRooms` in it).
- Keys use `ServerRoomId` (stable across map edits), plus direction for exits.
- Persistence: two small binary files per server and character, not one profile. The known-room
  history (`KnownRoomsStore`, `.bin`, a sorted `uint32` array, so no assumption about the ID
  range) is re-fetchable from the server. The discovered-exit list (`DiscoveredExitsStore`,
  `.exits`) is client-only and irreplaceable, so it is kept apart: a completed room list can
  never touch it, and a damaged file is renamed to `.corrupt` (or the next free `.corrupt.N`)
  rather than overwritten. Both use `QSaveFile` on the GUI thread behind a 2 s single-shot
  timer, flushed on destruction. This removes #566's worker threads.
- Precedence: live server data overwrites cache and transient state. A completed
  `Room.Known.List` replaces the cache. The server omitting a hidden exit updates the door
  state but never deletes a discovered exit.

### 4.2 Visibility policy

One pure-C++ policy answers `visibility(room)` as Known, Frontier, Unknown, or Unspecified,
and provides an effective exit view that hides undiscovered secret exits. **Frontier is
computed through that exit view**, so a glimmer never marks a secret door. Every consumer calls
it.

**The policy does not call #566's `Snapshot::isUndiscovered()`.** That function fails open: it
returns `complete && !contains(id)`, so until the terminated list arrives nothing counts as
undiscovered. Explore needs the opposite. The policy computes visibility itself: a room is
visible if it is in the cache, in the chunks received so far, in the live adds, or is the
current room. Everything else is fogged.

**A partial list is trusted for additions, never removals.** Only a completed
`Room.Known.List` removes stale IDs. So rooms appear as chunks arrive, instead of a new player
facing a fully fogged map for a minute or more.

## 5. Rendering

- **Fog as material.** A small R8 visibility mask (one texel per room cell, per z-layer, with a
  second channel for a reveal timestamp) is sampled by the room-cell shader (terrain, trails,
  overlays, walls and doors) and, later, the font shader. Connection lines do not sample it (see
  'Connection lines'). Layer fading and depth keep working, no extra pass. Patch with
  `glTexSubImage`: no remeshing on discovery.
- **Look.** Noise-threshold dissolve (hand-inked edges), parchment tint, optional time-of-day
  colour coupling; reuse the weather noise texture.
- **Connection lines (decided).** A connection, and the door name on it, is meshed only when
  every room it touches is in the base `visible` set (`exploreConnectionShown()`, checked where
  `ConnectionDrawer` emits geometry). The set is the service's base visibility, which does not
  depend on where the player stands, so stepping never remeshes. Undiscovered base-map rooms are
  in it, so a line from a discovered to an undiscovered room still draws in full. This is done on
  the CPU and not with a fragment test of the fog mask, because the mask keeps empty cells
  shown on purpose (so long lines between shown rooms are not cut): a line between two hidden
  rooms crosses mostly empty cells and its middle would show where they are. A shader fix would
  need both end cells on every vertex. A visibility set that only grows is meshed in the
  background while the old meshes keep drawing; one that loses rooms, or starts Explore, drops
  the meshes at once (`exploreVisibleOnlyGrows()`). On the shipped map, with nothing known or
  found, this culls about 36% of the connection triangle vertices and 38% of the quad vertices.
  Follow-up: infomarks and labels over hidden cells use a different batch path and are not
  culled yet.
- **Hidden exits.** The effective exit view runs in `visitRoom()` and `Connections.cpp`.
  Until Phase 4, finding a secret exit starts a remesh in the background while the current
  meshes keep drawing (`exploreExitsOnlyReveal()`; a change that could hide something still
  drops the meshes at once). That is correct but slow: the door shows only after the whole map
  is meshed again. Phase 4 removes the remesh for the door itself (see the Phase 4 row).
- **Doors (all modes).** A stable door index per door quad; a small state texture or UBO holds
  `(target state, start time)`; the vertex shader swings the leaf about its hinge with ease-out,
  a slam bounce, and a splinter shake for broken doors. Mostly procedural.
- Must compile for desktop GL and WebGL (ES 3.0).

## 6. Spoiler-surface audit

Canvas meshes, connections, room and door names, infomarks, labels, tooltips, room panel, Find
Rooms, selection, path finding and speedwalk, `_dirs` output, zoom-to-fit and scroll extents,
group markers, exports, and **Offline mode's room descriptions and walking** (see section 2).

## 7. Phases (and which modes see changes)

| Phase | Work | Modes affected |
|---|---|---|
| 0 | **Landed on this branch.** Unchanged #566 port, then: no default negotiation, `KnownRoomsStore`, fail-closed semantics, `KnownRoomsTracker` (disabled by default). Coordinating with the #566 author is a separate, human step. | none (one parser change, see below) |
| 1 | **Landed on this branch** (read-only enforcement moved to Phase 2): the tracker (runs while Explore is the mode), `EXPLORE` mode, the visibility policy and effective exit view (`src/map/ExplorerVisibility`), and discovered-exit recording and persistence. The policy has no consumer yet, and its `placedRooms` input is defined but nothing feeds it. | Explore |
| 2 | **Started.** Landed: the visibility service and the first consumer, undiscovered secret exits meshed as plain walls (`hideUndiscoveredExits`, applied to the map on the mesh worker thread). Find Rooms now reports only shown rooms, and no route through a hidden room. Still to do, most-leaking first: names and infomarks (these are meshed, so they need the Phase 3 mask rather than a remesh per step), room panel and tooltips, `_dirs`/speedwalk path finding, and (done, not yet seen in the app) read-only enforcement. | Explore |
| 3 | **Started.** Landed: the pure mask builder (`src/map/ExplorerFogMask`, tested), and the fog in the room-cell vertex shader (`room/tex/acolor`), which covers terrain, trails, overlays, walls and doors in one place. The service builds the base mask on its worker; `ExplorerFogTexture` uploads it (R8 2D array, nearest, alignment 1, limits checked, fail closed) and patches the player's cell each step. Hinted (frontier) cells are hidden until Phase 5. Not yet visually verified: the shader compiles and links under Mesa and passes glslang for GL 330 and ES 300, but nobody has looked at it on a real map. Landed: connection lines and door names are culled at mesh time (see 'Connection lines' under section 5). Next, in order: infomarks and labels over hidden cells (a separate batch path, still drawn); preset dropdown. Ship Severe (discard) first. Fog MVP: mask builder, shaders, preset dropdown. (The experimental flag was removed early, at the user's request.) | Explore |
| 4 | `DoorTracker` and its GMCP wiring; door animation: door state texture, swing. **A secret exit is a door state**, so finding one needs no remesh (see below). | **all modes** |
| 5 | Frontier, reveal animation, reduce motion, docs | Explore (reduce motion: all) |

Phase 4 (including `DoorTracker`) does not depend on Explore and can ship first.

**Phase 4 design for secret doors.**
- Doors and walls are `RoomQuadTexVert` quads on the same instanced room shader that reads the
  fog mask (`room/tex/acolor`): `w = colorId << 8 | tex_z`, bits 16 to 31 are free, and the wall
  orientation is part of `tex_z`. A door is therefore identified by its cell and orientation;
  no per-quad index is required.
- In Explore, mesh every secret exit twice, as a wall quad and as a door quad. The vertex
  shader drops the one that does not match the door's state, as the fog shader drops a quad.
- Door states are closed, open, broken and undiscovered secret, in a small state texture keyed
  by cell and direction (a plain 2D texture or a texture buffer; if an array texture is
  unavoidable, use only the layer-aware upload calls). Finding a secret is one texel patch plus
  the swing. Fail closed: an unbound or missing texture draws a secret as a wall.
- Connection lines and door names are separate meshes. Until their shaders read the state they
  stay on the background remesh, which is safe because nothing leaks before the find.
- `hideUndiscoveredExits` stays for everything that is not a mesh (Find Rooms, path finding,
  the room panel).
- **Slice A (instant reveal), in order.** (1) The found-secret mask (`ExplorerFoundMask`,
  `ExplorerSecrets`): one byte per cell with a bit per direction, both sides of a door, built on
  the visibility worker beside the fog mask; `foundSecretTexels()` gives the texels one new find
  sets. (2) The tag in bits 16 to 20 of the instance word (`RoomQuadTag.h`: form door/wall and
  direction) and the colour decode `(w >> 8) & 0xFF`. (3) `ExplorerFoundTexture`: an R8UI 2D
  array texture on its own unit (`FOUND_TEXTURE_UNIT = 2`), uploaded and patched only with the
  layer overloads of `QOpenGLTexture`, and the room shader that keeps the door form where the
  bit is set and the wall form where it is not (an unbound texture means not found). A find is
  patched into the texture at once (`MainWindow::patchFoundSecrets`), without waiting for the
  worker; patches only set bits and are re-applied on top of every newer base. The sampler is
  declared `highp`: GLSL ES has no default precision for integer array samplers (the ES prefix
  only declares one for `sampler2DArray`). (4) The mesher (`visitRoomExits()` in
  `MapCanvasRoomDrawer.cpp`) emits both forms for each secret exit when Explore passes it the real
  map (`VisitRoomOptions::secretSource`): the door form is what the real exit draws once found
  (wall, door, up/down marker, from its real data), the wall form is the plain wall it draws
  until then (nothing for up and down). Streams, connection lines and door names are not tagged
  and still come from the map with undiscovered secrets removed, so they appear with the
  background remesh. Outside Explore nothing is tagged and the output is identical to before.
  Checked on the shipped map (29,939 rooms): the 2,822 hidden exits are exactly the tagged ones
  (2,055 horizontal with one wall-form quad each, 767 vertical with none; 2 to 3 door-form quads
  each), the door form equals the old drawing of the real exit, the wall form equals the old
  drawing of the exit with the secret removed, and every other call is unchanged.
- There is no QML canvas in this tree (`libmm_qml_canvas.a` in an old build directory has no
  target), and the room shader has no ES or wasm variant: all of them use
  `legacy/room/tex/acolor`, so one shader covers desktop GL, ES 3.0 and WebGL2. Not yet run on ES
  or WebGL2.
- The rooms behind a found secret appear after the visibility recompute (about 0.85 s on the
  shipped map). An incremental flood from the found exit that patches the fog mask directly is a
  possible later step.

**Gating:** until phase 3 lands, Explore would show the whole map with hidden labels and
secret exits, and leaving it would warn "This will show the full map." That is incoherent for
users, so the flag that hid the menu entry and `_mode explore` was removed at the user's request, accepting
that Explore is incoherent until the fog exists.

## 8. Tests

- Policy matrix: each visibility and exit state against each consumer.
- Precedence: cache before and after a live message.
- Tracker tests with an injectable clock: TTL expiry, re-hide on omission, snap versus animate,
  bulk load does not animate.
- Frontier never marks an undiscovered secret exit.
- Mask builder; a per-draw-path leak test for every shader drawing map content.
- Mode tests: read-only, leave-Explore confirmation, persistence, `Room.Known` absent.

## 9. Risks

- Fog sampled in material shaders must composite per layer; needs a spike.
- A missed shader leaks a hint; the per-draw-path test mitigates it.
- Mask size on very large maps.
- Browser builds have no durable file store until an IndexedDB layer exists.
- Fixing a stale map needs Map mode, which reveals everything.
- Bundled map loaded read-only in Explore, and what happens to a user's custom map.

## 10. Phase 0 notes for Phase 1

- **Core.Supports.Set resets the module.** If the player's own MUD client sends
  `Core.Supports.Set`, `MudTelnet::resetGmcpModules()` drops `Room.Known`. The tracker re-adds
  it before each list request, but a set arriving mid-transfer can still interrupt that
  transfer. Phase 1 should re-request when that happens.
- **Behaviour change outside Explore.** The ported `JsonDoc.h` change makes top-level `null`,
  `true`, and `false` parse as scalars for all GMCP, not only `Room.Known`. It is small and the
  suite passes, but it is not strictly neutral.
- **Shutdown order.** `~MainWindow` deletes the connection listener (and so the `Proxy`, which
  fires the disconnect hooks) in its body, while its child objects, including the tracker, are
  still alive. New MainWindow-owned components hooked to `GameObserver` rely on this.
- **Explore is read-only.** `PathMachine::scheduleAction()` used to apply its change list in
  every mode except Offline. In Explore it now applies only `SetServerId` changes (see section
  3.2); Play, Map and Offline are unchanged. The map itself refuses all other edits while
  Explore is the mode, and `MainWindow::syncEditAccess()` disables the creation tools, the
  selection-based edit actions, the force-room action and undo/redo, switching an armed
  creation tool back to "Move map". Edits made by parser commands (`_set ...`) go through the same
  gate and are refused; the command reports "execute failed".
- **Non-blocking:** Explore reuses `map.png` as its icon, so its
  button looks like Map mode's; and `exploreModePersistenceTest` should snapshot and restore the whole
  configuration through `writeTo`/`readFrom` instead of two fields.
- **Server assumption, verified in the server source** (`add_gmcp_exit` and `can_see_exit` in
  `commands/act.enviro.c`). A closed hidden exit is sent with the `hidden` flag only if the
  character knows it (`knows_hidden_exit`); otherwise it is sent as `NOT_SHOWN` with no flags,
  no keyword and no target id. So `recordHiddenExits` records finds correctly. Two cases to
  know: an open hidden door is visible to everyone and carries the flag (so seeing it open
  counts as finding it), and a wizlit god sees every exit. Not yet confirmed against the live
  mume.org server, which may differ from this source.
- **How modification works, and the read-only decision (decided).** There is no map autosave
  (only remote-edit drafts autosave). `MapFrontend::isModified()` is `m_current != m_saved`, a
  structural comparison, so any path-machine edit made the window modified, enabled Save and
  made `maybeSave` prompt "save changes?" on close, new and open, as it still does in Play. The
  user decided: block all edits except server-ID backfill, never offering the map for saving.
  Implemented by re-baselining the saved map with each allowed backfill (see section 3.2),
  before `sig_onDataChanged` fires, because the title bar and `maybeSave` read `isModified()`.
- **Explore-mode secret exits are removed at one choke point.** Mesh generation reads a map in
  which undiscovered hidden exits are already removed, instead of patching each exit read in
  `visitRoom` and `Connections`. This covers walls, connection lines, flows, door names and
  the other side of the door. It relies on the unverified `"hidden"` assumption above: if the
  server lists unfound secret exits, nothing is hidden, which is never worse than today.

## 11. Handoff to the next session

State: branch `claude/affectionate-thompson-b2n9aj`, everything pushed. Phase 0 and Phase 1 are
landed; Phase 2 has started. The experimental flag has been removed: Explore is always available in the menu, toolbar and `_mode explore`. Until the fog (Phase 3) exists it shows the whole map except undiscovered secret exits.

Next slice, in order:
1. Route consumers through `ExplorerView`. Done: Find Rooms (no unit test, it is dialog code; check by hand). Next: room panel and tooltips, `_dirs`/speedwalk path finding. Room and door names and infomarks are meshed, so they wait for the Phase 3 mask.
2. Read-only enforcement: done (see sections 3.2 and 10). Not seen in the app yet: check that
   no modified star or save prompt appears while exploring, and that the creation tools are
   greyed out with the tooltip.
3. Re-request `Room.Known` after a client `Core.Supports.Set` (after the live check).
4. Non-blocking: Explore shares Map mode's toolbar icon; `exploreModePersistenceTest` restores only one
   config fields instead of snapshotting the whole config.

Still needed from the user: the live check against real MUME (list chunks arrive; an unfound
secret door is not in `Room.Info`; after finding it, it is listed with `"hidden"`; in Explore it
draws as a plain wall until found).

Working notes that cost time before:
- **Revealing a secret no longer blanks the map.** The canvas keeps the old meshes and remeshes
  in the background when a change can only show more; any change that could hide a secret still
  drops the meshes immediately. Measured on the shipped map: rewriting the 2,822 hidden exits
  takes about 160 ms and the visibility recompute about 850 ms, both off the GUI thread. The
  mesh generation and the GL upload were not measured; Phase 4 removes them from the path.
- Build per `AGENTS.md`: Debug, ccache, `-DWITH_MAP=OFF -DWITH_IMAGES=OFF -DWITH_AUDIO=OFF
  -DWITH_UPDATER=OFF -DWITH_WEBSOCKET=OFF -DWITH_OPENSSL=OFF`. Qt 6.4 and `libimmer-dev` come from
  apt (the immer download is blocked by the proxy); the environment setup script installs them.
- Run the full build and the full suite before every push (`QT_QPA_PLATFORM=offscreen ctest`, otherwise every test aborts). When filtering build output, match
  `error:` (a filter on " error " hides real failures and leaves a stale test binary).
- A 2D array texture upload with `QOpenGLTexture::setData(x, y, z, w, h, depth, ...)` writes only
  layer 0, and a one-texel patch lands on layer 0 too (found when Explore showed nothing on a
  5-layer map). Use the `layer` overloads. `ExplorerFogMask` layers are z levels, so every level
  but the lowest was hidden.
- Check tests by deliberately breaking the code and confirming a failure; several first-try
  passes were vacuous. Settings keys live under a `General` group in the settings file.
- Format with `clang-format-18 -i --style=file` on changed `.h`/`.cpp` files only.
