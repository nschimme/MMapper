// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MazeDetector.h"

#include "../map/ExitDirection.h"
#include "../map/RawExit.h"
#include "../map/RoomHandle.h"

#include <algorithm>
#include <stack>

MazeInfo MazeDetector::detectMazes(const Map &map)
{
    MazeInfo info;
    const auto &allRooms = map.getRooms();
    if (allRooms.empty()) {
        return info;
    }

    std::unordered_set<RoomId> seedRooms;
    std::unordered_map<RoomId, std::vector<RoomId>> adj;

    // Build adjacency and detect initial seed features
    for (const RoomId v : allRooms) {
        const RoomHandle rh = map.findRoomHandle(v);
        if (!rh.exists()) {
            continue;
        }

        adj[v] = {}; // Ensure v is present in adj map

        for (const ExitDirEnum dir : ALL_EXITS7) {
            const RawExit &exit = rh.getExit(dir);
            for (const RoomId u : exit.getOutgoingSet()) {
                if (!allRooms.contains(u)) {
                    continue;
                }
                adj[v].push_back(u);

                if (u == v) {
                    info.selfLoops.insert(DirectedExitKey{v, dir});
                    seedRooms.insert(v);
                } else if (isNESWUD(dir)) {
                    const Coordinate expectedPos = rh.getPosition() + exitDir(dir);
                    const RoomHandle targetRh = map.findRoomHandle(u);
                    if (targetRh.exists() && targetRh.getPosition() != expectedPos) {
                        seedRooms.insert(v);
                        seedRooms.insert(u);
                    }
                }

                // Check repeating room descriptions
                const RoomHandle targetRh = map.findRoomHandle(u);
                if (targetRh.exists()) {
                    const auto descV = rh.getDescription();
                    const auto descU = targetRh.getDescription();
                    if (!descV.isEmpty() && descV == descU) {
                        seedRooms.insert(v);
                        seedRooms.insert(u);
                    }
                }
            }
        }
    }

    // Tarjan's SCC algorithm
    std::unordered_map<RoomId, int> dfn;
    std::unordered_map<RoomId, int> low;
    std::unordered_set<RoomId> onStack;
    std::stack<RoomId> st;
    int timer = 0;
    std::vector<std::vector<RoomId>> sccs;

    auto tarjan = [&](auto &self, const RoomId u) -> void {
        dfn[u] = low[u] = ++timer;
        st.push(u);
        onStack.insert(u);

        auto it = adj.find(u);
        if (it != adj.end()) {
            for (const RoomId v : it->second) {
                if (!dfn.contains(v)) {
                    self(self, v);
                    low[u] = std::min(low[u], low[v]);
                } else if (onStack.contains(v)) {
                    low[u] = std::min(low[u], dfn[v]);
                }
            }
        }

        if (low[u] == dfn[u]) {
            std::vector<RoomId> scc;
            while (true) {
                const RoomId node = st.top();
                st.pop();
                onStack.erase(node);
                scc.push_back(node);
                if (node == u) {
                    break;
                }
            }
            sccs.push_back(std::move(scc));
        }
    };

    for (const RoomId v : allRooms) {
        if (!dfn.contains(v)) {
            tarjan(tarjan, v);
        }
    }

    // Filter SCCs into maze clusters
    uint32_t clusterCounter = 1;
    for (const auto &scc : sccs) {
        bool isMaze = false;
        if (scc.size() == 1) {
            const RoomId v = scc.front();
            if (seedRooms.contains(v)) {
                // Size 1 with self loop or explicit seed feature
                isMaze = true;
            }
        } else if (scc.size() >= 2) {
            // Check if SCC contains any seed room or forms a cycle loop
            for (const RoomId v : scc) {
                if (seedRooms.contains(v)) {
                    isMaze = true;
                    break;
                }
            }
            // If SCC size >= 3 or has multiple edges, also treat as maze cluster
            if (scc.size() >= 3) {
                isMaze = true;
            }
        }

        if (isMaze) {
            const uint32_t cId = clusterCounter++;
            for (const RoomId v : scc) {
                info.mazeRooms.insert(v);
                info.roomClusterId[v] = cId;
            }
        }
    }

    // Categorize connections for all rooms in maze clusters
    for (const RoomId v : info.mazeRooms) {
        const uint32_t cId = info.roomClusterId[v];
        const RoomHandle rh = map.findRoomHandle(v);
        if (!rh.exists()) {
            continue;
        }

        for (const ExitDirEnum dir : ALL_EXITS7) {
            const RawExit &exit = rh.getExit(dir);
            for (const RoomId u : exit.getOutgoingSet()) {
                const DirectedExitKey key{v, dir};
                if (u == v) {
                    info.selfLoops.insert(key);
                }
                const auto it = info.roomClusterId.find(u);
                if (it != info.roomClusterId.end() && it->second == cId) {
                    info.internalMazeConnections.insert(key);
                } else {
                    info.mazeExitConnections.insert(key);
                }
            }
        }
    }

    return info;
}
