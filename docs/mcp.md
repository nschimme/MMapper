# MCP server

MMapper can run an [MCP](https://modelcontextprotocol.io) server so tools and tests can look at
and drive a running MMapper: read its state and what the game sent, and (as more tools are added)
load maps, send input and inspect the map. It is meant for developing MMapper and for developing
the MUD it maps, not for playing the game for you.

It is built unless you configure with `-DWITH_MCP=OFF`, and it is **off by default** when
running.

## Turning it on

*Preferences → MCP Server*: tick **Enable the MCP server**, pick a port (default 4280) and copy
the access token. Changes take effect immediately. If the port is busy the page says so and
MMapper keeps running.

The server listens on `127.0.0.1` (and `::1` where available) only, speaks HTTP, and has a
single endpoint, `POST /mcp`. Every request needs `Authorization: Bearer <token>`.

```
claude mcp add --transport http mmapper http://127.0.0.1:4280/mcp \
    --header "Authorization: Bearer <token>"
```

Use `127.0.0.1` in URLs rather than `localhost`: some systems resolve `localhost` to `::1` first.

## Security

* Anyone with the token can make MMapper act on your behalf, including sending commands to the
  game. The token is stored in plain text in MMapper's settings file.
* Requests must carry a `Host` of `localhost`, `127.0.0.1` or `[::1]` with the server's port, and
  an `Origin` (if present) that is a loopback web origin. This stops web pages, including ones
  using DNS rebinding, from talking to the server even if they could guess the port.
* MMapper only ever starts a connection to the MUD in two places: when a session is created, and
  when the `_connect` command runs. Both are checked here: unless MMapper is in OFFLINE mode,
  `open_session` and any `_connect` line (through `send` or `mmapper_connect`) are refused when the
  configured MUD host is not this computer. Switching mode with `_config mode` does not connect by
  itself, and the check uses the mode and host at that moment, so a later `_connect` is still
  refused. No MMapper command sets the MUD host; it only comes from the settings file (or the
  Preferences window), which is why a test profile must set it, and why `get_status` reports it.
  This protects against accidents in a test setup; it is not a sandbox against someone who can
  already change MMapper's settings.
* `GET` is not supported (there are no server-initiated messages); JSON-RPC batches are rejected.

## Tools

`tools/list` is the exact reference: every tool's description and arguments come from the code, so
they cannot go out of date. This is what the groups are for.

* **State**: `get_status` (call it first; it reports what MMapper is actually configured to do and
  which session exists) and `read_events` (what happened since a cursor: text shown to the user,
  prompts, GMCP, what was sent to the MUD, connection changes, position changes and map loads).
* **Session**: `open_session` and `close_session` start and end a client session without a telnet
  client, because the map, parser and MUD connection hang off one. A session that belongs to a real
  client is never replaced or closed.
* **Driving it**: `send` types a line as the user would and can wait for the next prompt or for
  matching text; `send_gmcp` sends GMCP to the MUD.
* **Replaying a MUD**: `inject_mud` and `inject_gmcp` feed MMapper bytes and GMCP as if the MUD had
  sent them, so recorded sessions can be replayed without one.
* **Map**: `load_map` replaces the map with a file and waits for it, and `get_room` describes a
  room. `map_summary`, `list_rooms` (filter by area, name or bounding box, paged) and `dump_map`
  (every room as sorted JSON in a file, for diffing) inspect the whole map and need no session.
* **MMapper commands**: one `mmapper_<command>` tool for every command typed with the command
  prefix (for example `mmapper_search`, `mmapper_dirs`, `mmapper_room`). They are generated from the
  parser's command table, so the list is complete before a session exists and follows the parser
  when commands are added. Each is the same as `send` with the line `_<command> <args>`; output a
  command produces later shows up in `read_events`.

### What creates rooms

MMapper builds room events from GMCP, not from the XML text: to make it see a room, inject a
`Room.Info` message and then a prompt, for example

```
inject_gmcp  Room.Info  {"id":1,"name":"The Cottage","area":"The Shire","desc":"A small cottage.",
                         "environment":"building","exits":{"n":{"id":2}}}
inject_mud   "<prompt>!&gt;</prompt>"   (prompt: true)
```

and `Event.Moved` with `{"dir":"north"}` before the next `Room.Info` to walk. In **MAP** mode this
builds the map. In **OFFLINE** mode the position is tracked against the map you have loaded but
rooms are never added or changed, so use MAP mode (with the MUD host set to this computer) to test
map building and OFFLINE to test navigating an existing map. Injected XML text only affects what is
displayed; it must be well-formed (a malformed stream can trip an assertion in a debug build).

## Testing against a MUD server of your own

When Qt has SSL support (the usual case) MMapper only ever connects to a MUD over **TLS**; the
plain-TCP fallback exists only for builds without SSL. So a local test MUD must speak TLS. A
self-signed certificate is fine (MMapper does not abort on a certificate it cannot verify). For a
telnet server to work as a MUD for MMapper it should negotiate GMCP (`IAC WILL GMCP`) if you want
`send_gmcp`, and end each prompt with `IAC GA`, which is what `send` with `wait_for: "prompt"`
waits for.

Set the profile's `Server name` and `Remote port number` (in `[Connection]`) to the test server,
use PLAY or MAP mode, and call `open_session` with `wait_for_mud: true`. Only a MUD host on this
computer is accepted outside OFFLINE mode.

## Running MMapper for tests

Point `MMAPPER_PROFILE_PATH` at a settings file so a test run never touches your real settings.
The file **must already exist** and be writable, otherwise MMapper silently uses your normal
settings. MMapper rewrites the file when it exits.

**Beware the section name.** The group named `General` is stored as `[%General]`, and spaces in
key names are written as `%20`. A hand-written `[General]` section is silently ignored, and
MMapper then runs with defaults: play mode, connecting to the live game, with update checks on.
Check `get_status` (or the log) before sending anything.

A profile for an offline test run with the MCP server on:

```ini
[%General]
Map%20Mode=2
Check%20for%20update=false
Run%20first%20time=false

[Auto%20load%20world]
Auto%20load=false

[Connection]
Local%20port%20number=14243

[MCP]
Enabled=true
Port=14280
Token=choose-a-long-random-string
```

`Map%20Mode` is `0` play, `1` map, `2` offline. Offline mode never connects to the game. Use a
different `Local port number` and MCP `Port` per parallel run. Keep `Auto load` off: it is **on**
by default, and an autoloaded map races with anything your test loads.

Not every Qt build has the `offscreen` platform plugin; where it does,
`QT_QPA_PLATFORM=offscreen` runs MMapper without a window (the CI does this).
