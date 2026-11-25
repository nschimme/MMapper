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
* `GET` is not supported (there are no server-initiated messages); JSON-RPC batches are rejected.

## Tools

| Tool | What it does |
|------|--------------|
| `get_status` | Effective map mode, MUD host and port, update-check setting, settings file in use, whether a client session is attached, current room. **Call this first.** |
| `read_events` | Events since a cursor: `mud_text`, `user_text` and `gmcp`. Returns `nextSeq` to pass as `since` next time, and `truncated` if older events were dropped. |

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
