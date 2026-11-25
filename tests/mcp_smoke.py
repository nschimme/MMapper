#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019 The MMapper Authors
"""Smoke test for MMapper's MCP server, run by hand against a running MMapper.

CI cannot start the full application (it needs an OpenGL context), so this is not part of ctest.
See "Running MMapper for tests" in docs/mcp.md for starting MMapper under Xvfb with the right
profile (MAP mode, MCP enabled, auto load off, MUD host set to this computer).

    tests/mcp_smoke.py --port 14280 --token <token> [--workdir DIR]

It builds a three-room map from injected GMCP, then checks the map tools (map_summary,
list_rooms, dump_map), the room ids, save_map in every format and loading the copies back,
wait_for_event, load failures that must not hang, and screenshot. It starts from an empty map,
replaces the map, and writes its files into --workdir (a fresh temporary directory by default).
It also changes MMapper's own state: it turns the debug timing overlay off (`_config map
perf-stats`, which rewrites the settings file) and leaves the canvas on the player's layer. Run it
against a test profile, never your real settings. Exits non-zero on any failure.
"""

import argparse
import hashlib
import json
import os
import shutil
import tempfile
import threading
import time
import urllib.request

FAILURES = []


class Mcp:
    def __init__(self, port, token):
        self.url = f"http://127.0.0.1:{port}/mcp"
        self.token = token

    def rpc(self, method, params=None):
        body = {"jsonrpc": "2.0", "id": 1, "method": method}
        if params is not None:
            body["params"] = params
        req = urllib.request.Request(
            self.url,
            data=json.dumps(body).encode(),
            headers={"Authorization": f"Bearer {self.token}", "Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=120) as r:
            return json.load(r)

    def call(self, tool, /, **args):
        return self.rpc("tools/call", {"name": tool, "arguments": args})["result"]

    def data(self, tool, /, **args):
        """The structured result of a tool call that must succeed."""
        r = self.call(tool, **args)
        if r.get("isError"):
            raise RuntimeError(f"{tool} failed: {r['content'][0]['text']}")
        return r.get("structuredContent")

    def say(self, tool, args):
        """Run an mmapper_<command> tool; returns the text MMapper showed."""
        r = self.call(tool, args=args)
        events = (r.get("structuredContent") or {}).get("events", [])
        return [e["text"].strip() for e in events if e["kind"] == "user_text" and e["text"].strip()]


def check(label, ok, detail=""):
    print(("ok    " if ok else "FAIL  ") + label + (f"  [{detail}]" if detail and not ok else ""))
    if not ok:
        FAILURES.append(label)


ROOMS = {
    1: ("The Cottage", "The Shire", "building", {"n": {"id": 2}}),
    2: ("A Lane", "The Shire", "road", {"s": {"id": 1}, "e": {"id": 3}}),
    3: ("The Mill", "Bree", "building", {"w": {"id": 2}}),
}


def visit(m, room_id, move=None):
    name, area, env, exits = ROOMS[room_id]
    if move:
        m.data("inject_gmcp", package="Event.Moved", json={"dir": move})
    m.data(
        "inject_gmcp",
        package="Room.Info",
        json={"id": room_id, "name": name, "area": area, "desc": f"Desc of {name}.",
              "environment": env, "exits": exits},
    )
    m.data("inject_mud", text="<prompt>!&gt;</prompt>", prompt=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("--token", required=True)
    ap.add_argument("--workdir", help="directory for files the test writes (default: a new temp dir)")
    args = ap.parse_args()
    work = os.path.abspath(args.workdir or tempfile.mkdtemp(prefix="mmapper-mcp-smoke-"))
    os.makedirs(work, exist_ok=True)
    p = lambda name: os.path.join(work, name)  # noqa: E731
    m = Mcp(args.port, args.token)

    # --- status and setup
    status = m.data("get_status")
    check("get_status reports MAP mode (the test builds a map)", status["mapMode"] == "MAP",
          str(status.get("mapMode")))
    tools = {t["name"] for t in m.rpc("tools/list")["result"]["tools"]}
    for t in ("map_summary", "list_rooms", "dump_map", "save_map", "load_map", "wait_for_event",
              "screenshot", "mmapper_map", "mmapper_config"):
        check(f"tool {t} is listed", t in tools)
    m.data("open_session", timeout_ms=10000)

    # Start from an empty map even if an earlier run left one loaded: a load that fails still
    # discards the current map first (see load_map).
    open(p("empty-start.mm2"), "w").close()
    m.call("load_map", path=p("empty-start.mm2"), timeout_ms=20000)

    # --- build a map: walk the loop so every room is revisited (and so becomes permanent)
    for room_id, move in ((1, None), (2, "north"), (3, "east"), (2, "west"), (1, "south"),
                          (2, "north"), (3, "east")):
        visit(m, room_id, move)

    # --- map inspection
    s = m.data("map_summary")
    check("map_summary: 3 rooms", s["rooms"] == 3, str(s))
    check("map_summary: areas", s["areas"] == {"Bree": 1, "The Shire": 2}, str(s["areas"]))
    check("map_summary: unsaved changes, no file", s["modified"] and s["fileName"] == "")
    all_rooms = m.data("list_rooms")
    check("list_rooms: all three in id order",
          [r["name"] for r in all_rooms["rooms"]] == ["The Cottage", "A Lane", "The Mill"])
    check("list_rooms: area filter", [r["name"] for r in m.data("list_rooms", area="Bree")["rooms"]] == ["The Mill"])
    check("list_rooms: name filter is case-insensitive",
          [r["name"] for r in m.data("list_rooms", name="LANE")["rooms"]] == ["A Lane"])
    check("list_rooms: bounding box", m.data("list_rooms", min_y=1)["total"] == 2)
    page = m.data("list_rooms", limit=1, offset=1)
    check("list_rooms: paging", page["total"] == 3 and [r["name"] for r in page["rooms"]] == ["A Lane"])

    # --- ids: current room is The Mill (server id 3)
    cur = m.data("get_status")
    room = m.data("get_room")
    check("current room is The Mill by every route",
          room["name"] == "The Mill" and room["id"] == cur["currentRoomExternalId"]
          and room["internalId"] == cur["currentRoomId"] and room.get("serverId") == 3, str(cur))

    # --- dump, save in every format, load back, compare
    m.data("dump_map", path=p("before.json"), overwrite=True)
    again = m.call("dump_map", path=p("before.json"))
    check("dump_map refuses to overwrite", again.get("isError") is True)
    check("dump_map refuses a relative path", m.call("dump_map", path="x.json").get("isError") is True)
    for fmt, ext in (("mm2", "mm2"), ("xml", "xml"), ("mmp", "mmp")):
        r = m.call("save_map", path=p(f"copy.{ext}"), format=fmt, overwrite=True)
        check(f"save_map {fmt}", not r.get("isError") and os.path.getsize(p(f"copy.{ext}")) > 0,
              r["content"][0]["text"][:100])
    r = m.call("save_map", path=p("copy.base.mm2"), base_map=True, overwrite=True)
    check("save_map base map", not r.get("isError"), r["content"][0]["text"][:100])
    shutil.rmtree(p("copy.web"), ignore_errors=True)  # a web export needs a new or empty directory
    r = m.call("save_map", path=p("copy.web"), format="web")
    check("save_map web", not r.get("isError") and os.path.isdir(p("copy.web")),
          r["content"][0]["text"][:100])
    check("save_map refuses a web export into a non-empty directory",
          m.call("save_map", path=p("copy.web"), format="web", overwrite=True).get("isError") is True)
    check("save_map refuses to overwrite", m.call("save_map", path=p("copy.mm2")).get("isError") is True)
    after = m.data("map_summary")
    check("save_map leaves the map's file name and modified flag alone",
          after["fileName"] == "" and after["modified"] is True)
    for ext in ("xml", "mm2"):
        loaded = m.data("load_map", path=p(f"copy.{ext}"))
        check(f"load_map {ext}: 3 rooms", loaded["rooms"] == 3, str(loaded))
        m.data("dump_map", path=p(f"after.{ext}.json"), overwrite=True)
        check(f"dump after save and load ({ext}) equals the original",
              open(p("before.json")).read() == open(p(f"after.{ext}.json")).read())

    # --- loads that fail must come back as errors, not hang
    open(p("empty.mm2"), "w").close()
    with open(p("copy.mm2"), "rb") as f:
        head = f.read(100)
    with open(p("trunc.mm2"), "wb") as f:
        f.write(head)
    with open(p("copy.xml")) as f:
        xml = f.read()
    start, end = xml.index("<room "), xml.rindex("</room>") + len("</room>")
    with open(p("norooms.xml"), "w") as f:
        f.write(xml[:start] + xml[end:])
    for name in ("empty.mm2", "trunc.mm2", "norooms.xml"):
        t = time.time()
        r = m.call("load_map", path=p(name), timeout_ms=20000)
        check(f"load_map {name} fails fast", r.get("isError") is True and time.time() - t < 15,
              r["content"][0]["text"][:100])
    # --- internal and external ids that really differ. A load discards the map first, so the map
    # is empty now. Leave the first room temporary (visited once): a save does not keep temporary
    # rooms, so after a reload the surviving rooms are renumbered and the two kinds of id differ.
    check("the failed loads left an empty map", m.data("map_summary")["rooms"] == 0)
    for room_id, move in ((1, None), (2, "north"), (3, "east"), (2, "west"), (3, "east")):
        visit(m, room_id, move)
    m.data("save_map", path=p("ids.mm2"), overwrite=True)
    check("the reloaded map dropped the temporary room",
          m.data("load_map", path=p("ids.mm2"))["rooms"] == 2)
    st = m.data("get_status")
    check("internal and external ids differ for the current room",
          st["currentRoomId"] != st["currentRoomExternalId"], str(st))
    summ = m.data("map_summary")
    cur = m.data("get_room")
    by_ext = m.data("get_room", id=st["currentRoomExternalId"])
    check("get_status, map_summary and get_room agree on both ids",
          summ["currentRoomId"] == st["currentRoomId"]
          and summ["currentRoomExternalId"] == st["currentRoomExternalId"]
          and summ.get("currentRoomServerId") == st.get("currentRoomServerId") == 3
          and cur["internalId"] == st["currentRoomId"] and cur["id"] == st["currentRoomExternalId"]
          and by_ext["name"] == cur["name"] == "The Mill" and by_ext["internalId"] == st["currentRoomId"],
          str((st, summ, cur["id"], cur["internalId"])))
    check("list_rooms uses external ids",
          sorted(r["id"] for r in m.data("list_rooms")["rooms"]) == sorted(
              [st["currentRoomExternalId"], st["currentRoomExternalId"] - 1]))

    check("a good load works after failed ones", m.data("load_map", path=p("copy.mm2"))["rooms"] == 3)
    events = m.data("read_events", kinds=["map_load"], max=1000)["events"]
    check("map_load events include failures", any(not e["data"]["success"] for e in events))

    # --- ids again, after a reload; wait_for_event
    status = m.data("get_status")
    check("ids still agree after a reload",
          m.data("get_room", id=status["currentRoomExternalId"])["internalId"] == status["currentRoomId"])
    future = m.data("get_status")["lastEventSeq"] + 1000
    m.data("inject_mud", text="a line\r\n")
    check("wait_for_event with a cursor in the future ignores earlier events",
          m.data("wait_for_event", since=future, kinds=["user_text"], timeout_ms=300)["status"] == "timeout")
    check("wait_for_event times out", m.data("wait_for_event", kinds=["map_save"], timeout_ms=300)["status"] == "timeout")
    check("wait_for_event sees a logged event",
          m.data("wait_for_event", since=0, kinds=["map_load"], data={"success": True})["status"] == "matched")
    seq = m.data("get_status")["lastEventSeq"]
    box = {}
    t = threading.Thread(target=lambda: box.update(
        r=m.data("wait_for_event", since=seq, kinds=["user_text"], text_regex=r"hello\s+world", timeout_ms=8000)))
    t.start()
    time.sleep(1)
    m.data("inject_mud", text="say hello   world\r\n")
    t.join()
    check("wait_for_event sees a new event", box.get("r", {}).get("status") == "matched")

    # --- screenshot (needs a GL context; the timing overlay would make shots differ)
    m.say("mmapper_config", "map perf-stats set enabled false")
    time.sleep(2)
    m.call("screenshot")
    time.sleep(1)  # the first shot can still say "the map isn't ready yet"

    def shot(name):
        r = m.call("screenshot", path=p(name))
        ok = not r.get("isError") and any(c["type"] == "image" for c in r["content"])
        return ok, hashlib.md5(open(p(name), "rb").read()).hexdigest() if ok else None

    ok0, h0 = shot("layer0.png")
    ok0b, h0b = shot("layer0b.png")
    check("screenshot returns a PNG image", ok0)
    check("two shots of the same view are identical", ok0 and h0 == h0b)
    m.say("mmapper_config", "map layer down")
    time.sleep(1)
    check("a screenshot of another layer differs", shot("layer-1.png")[1] != h0)
    m.say("mmapper_config", "map layer reset")
    time.sleep(1)
    check("after layer reset the first view is back", shot("layer0c.png")[1] == h0)

    print(f"\nfiles written to {work}")
    if FAILURES:
        print(f"{len(FAILURES)} check(s) FAILED: " + "; ".join(FAILURES))
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
