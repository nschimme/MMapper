# Explorer Mode: Plan (v2.2, advisor-reviewed twice, decisions settled)

## 1. Goal

Ship a full MUME map, but let the player see only what their character has discovered. Unknown
rooms are hidden behind a tasteful fog of war whose strength the player controls. Secret exits
stay walls until found. Doors animate when they open and close (in every mode).

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
| Fail direction | Fail closed: anything whose status is unknown stays fogged. |
| Door reset | Every 60 minutes of **real time**; transient door state expires on that interval (monotonic clock, injectable for tests). |
| Name | "Explore" in menus and status, matching "Play" and "Map". |
| Offline mode | Stays a "show everything" mode. Switching to it from Explore warns "This reveals the full map." "Disconnected Explore" means using Explore while not connected, from the cached profile. |
| Rooms with no server ID | Fogged, unless the path machine has placed the player there this session. They have no stable key, so they cannot be persisted and fog again next session. |
| Group beacons on fogged rooms | The beacon is shown with the room drawn as a bare marker. Your own position is always shown. |
| #566 | Coordinate with its author, rebase onto master, and land the data layer first (the extension path, not a fork). |

**Frontier, decided:** glimpsed and visited are the same thing. `Room.Known` already includes
rooms seen through an exit, so it is the "visited" set, and frontier is computed from all known
rooms, which is what the policy does. No separate "entered" tracking is needed.

## 3. UX specification

### 3.1 Vocabulary (menus, status bar, settings, docs)

- **Explore mode**: the mode. Menu "Explore", status "Explore mode".
- **Fog**: the effect hiding undiscovered rooms. UI label "Fog".
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
- Explore is read-only: map edit tools are disabled with a tooltip ("Switch to Map mode to
  edit"). Tension, listed under risks: fixing a stale map needs Map mode, which reveals
  everything.
- No unsaved-changes prompt is added. Explore refuses to start edits, and entering it behaves
  like entering Play today. Before implementing, check what Play already does (the path
  machine can add `HIDDEN` flags in some paths) and make Explore at least as strict.

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

Preferences > Graphics, beside the weather sliders: a preset dropdown **Subtle / Medium /
Severe**. There is no "off" (that is Play). One separate switch, "Show frontier hints",
applies at every level: at Severe it is the only trace of unknown rooms.

Layer matrix (what an **undiscovered** room shows):

| Layer | Subtle | Medium | Severe |
|---|---|---|---|
| Terrain | dimmed, desaturated | flat parchment tint, noise-dissolved edge | dissolved |
| Mob and load overlays (herbs, treasure...) | hidden | hidden | hidden |
| Infomarks and labels | hidden | hidden | hidden |
| Exits, walls, doors | faint | hidden | hidden |
| Connections from a discovered room toward an undiscovered one | full line | stub to the cell edge | stub to the cell edge |
| Room and door names | hidden | hidden | hidden |
| Frontier glimmer | optional | optional | optional |

Subtle only softens the look of terrain; it never leaks overlays, marks, or names.

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
  second channel for a reveal timestamp) is sampled by the room, connection, wall, and font
  shaders. Layer fading and depth keep working, no extra pass. Patch with `glTexSubImage`: no
  remeshing on discovery.
- **Look.** Noise-threshold dissolve (hand-inked edges), parchment tint, optional time-of-day
  colour coupling; reuse the weather noise texture.
- **Hidden exits.** The effective exit view runs in `visitRoom()` and `Connections.cpp`. A
  reveal goes through a coalesced async remesh (rare).
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
| 1 | **Landed on this branch** (read-only enforcement moved to Phase 2): the tracker behind the flag (toggleable mid-session), `EXPLORE` mode, the visibility policy and effective exit view (`src/map/ExplorerVisibility`), and discovered-exit recording and persistence. The policy has no consumer yet, and its `placedRooms` input is defined but nothing feeds it. | Explore, behind an experimental flag |
| 2 | **Started.** Landed: the visibility service and the first consumer, undiscovered secret exits meshed as plain walls (`hideUndiscoveredExits`, applied to the map on the mesh worker thread). Still to do, most-leaking first: names and infomarks, Find Rooms, path finding, and read-only enforcement (decision pending). | Explore, behind the flag |
| 3 | Fog MVP: mask builder, shaders, preset dropdown. **Remove the flag here.** | Explore |
| 4 | `DoorTracker` and its GMCP wiring; door animation: door index, state texture, swing | **all modes** |
| 5 | Frontier, reveal animation, reduce motion, docs | Explore (reduce motion: all) |

Phase 4 (including `DoorTracker`) does not depend on Explore and can ship first.

**Gating:** until phase 3 lands, Explore would show the whole map with hidden labels and
secret exits, and leaving it would warn "This will show the full map." That is incoherent for
users, so the Explore menu entry and `_mode explore` stay behind a dev/experimental flag until
the fog exists.

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
- **Explore is not read-only yet.** `PathMachine::scheduleAction()` applies its change list in
  every mode except Offline, so Play, and therefore Explore, already lets the path machine
  backfill server IDs, add `HIDDEN` door flags and `NO_MATCH`, and update room fields. Manual
  edit tools are also not mode-gated. Read-only enforcement moves to Phase 2, with the other
  edit and spoiler surfaces. Tension to resolve then: blocking all edits also blocks server-ID
  backfill, which is what lets rooms without a server ID become persistable.
- **Non-blocking, before the flag comes off:** Explore reuses `map.png` as its icon, so its
  button looks like Map mode's; and `exploreModeFlagTest` should snapshot and restore the whole
  configuration through `writeTo`/`readFrom` instead of two fields.
- **Unverified server assumption.** `recordHiddenExits` assumes MUME lists a secret exit with the
  `"hidden"` flag in `Room.Info` only after the player has found it. If it lists unfound ones
  too, every secret door would be recorded as found. This needs a live check (see the report
  for this round) before the flag comes off.
- **Facts for the read-only decision.** There is no map autosave (only remote-edit drafts
  autosave). But the path machine's edits change the map, so `MapData::isModified()` becomes
  true: the window is marked modified, Save is enabled, and `maybeSave` prompts "save changes?"
  on close, new, and open. That already happens in Play today. On a shipped read-only map that
  would prompt players to overwrite it. Options: block all path-machine edits in Explore;
  allow only server-ID backfill; or leave Explore behaving like Play. Recommended: block all
  edits except server-ID backfill, applied to a copy that is never offered for saving, but this
  needs the user's decision because backfill is what makes rooms without a server ID persistable.
- **Explore-mode secret exits are removed at one choke point.** Mesh generation reads a map in
  which undiscovered hidden exits are already removed, instead of patching each exit read in
  `visitRoom` and `Connections`. This covers walls, connection lines, flows, door names and
  the other side of the door. It relies on the unverified `"hidden"` assumption above: if the
  server lists unfound secret exits, nothing is hidden, which is never worse than today.

## 11. Handoff to the next session

State: branch `claude/affectionate-thompson-b2n9aj`, everything pushed. Phase 0 and Phase 1 are
landed; Phase 2 has started. Explore mode is behind a hidden flag (`Explorer mode enabled
(experimental)` under `[General]`, default false), so nothing changes for normal users.

Next slice, in order:
1. Route consumers through `ExplorerView` (the service's first real reader), most-leaking first:
   room names and door names, infomarks, Find Rooms, then path finding and speedwalk.
2. Read-only enforcement, after the user decides between: block all path machine edits in Explore;
   backfill server IDs only; or behave like Play (see section 10 for the save-prompt facts).
3. Re-request `Room.Known` after a client `Core.Supports.Set` (after the live check).
4. Non-blocking: Explore shares Map mode's toolbar icon; `exploreModeFlagTest` restores only two
   config fields instead of snapshotting the whole config.

Still needed from the user: the live check against real MUME (list chunks arrive; an unfound
secret door is not in `Room.Info`; after finding it, it is listed with `"hidden"`; in Explore it
draws as a plain wall until found), and the read-only decision.

Working notes that cost time before:
- Build per `AGENTS.md`: Debug, ccache, `-DWITH_MAP=OFF -DWITH_IMAGES=OFF -DWITH_AUDIO=OFF
  -DWITH_UPDATER=OFF -DWITH_WEBSOCKET=OFF -DWITH_OPENSSL=OFF`. Qt 6.4 and `libimmer-dev` come from
  apt (the immer download is blocked by the proxy); the environment setup script installs them.
- Run the full build and the full suite before every push. When filtering build output, match
  `error:` (a filter on " error " hides real failures and leaves a stale test binary).
- Check tests by deliberately breaking the code and confirming a failure; several first-try
  passes were vacuous. Settings keys live under a `General` group in the settings file.
- Format with `clang-format-18 -i --style=file` on changed `.h`/`.cpp` files only.
