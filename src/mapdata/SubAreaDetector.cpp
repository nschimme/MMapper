// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "SubAreaDetector.h"

#include "../map/ExitDirection.h"
#include "../map/RawExit.h"
#include "../map/RoomHandle.h"

#include <algorithm>
#include <stack>

#include <queue>

SubAreaInfo SubAreaDetector::detectSubAreas(const Map &map)
{
    SubAreaInfo info;
    const auto &allRooms = map.getRooms();
    if (allRooms.empty()) {
        return info;
    }

    std::unordered_set<RoomId> seedRooms;
    std::unordered_map<RoomId, std::vector<RoomId>> adj;
    std::unordered_map<RoomId, std::vector<RoomId>> revAdj;
    std::unordered_map<RoomId, int> inDegree;
    std::unordered_map<RoomId, int> outDegree;

    // Build adjacency and detect seed features
    for (const RoomId v : allRooms) {
        const RoomHandle rh = map.findRoomHandle(v);
        if (!rh.exists()) {
            continue;
        }

        adj[v] = {};
        inDegree[v] = inDegree[v]; // Ensure entry exists
        outDegree[v] = 0;

        for (const ExitDirEnum dir : ALL_EXITS7) {
            const RawExit &exit = rh.getExit(dir);
            for (const RoomId u : exit.getOutgoingSet()) {
                if (!allRooms.contains(u)) {
                    continue;
                }
                adj[v].push_back(u);
                revAdj[u].push_back(v);
                outDegree[v]++;
                inDegree[u]++;

                if (u == v) {
                    info.selfLoops.insert(DirectedExitKey{v, dir});
                    seedRooms.insert(v);
                } else if (isNESWUD(dir)) {
                    const Coordinate expectedPos = rh.getPosition() + exitDir(dir);
                    const RoomHandle targetRh = map.findRoomHandle(u);
                    if (targetRh.exists()) {
                        if (targetRh.getPosition() != expectedPos) {
                            seedRooms.insert(v);
                            seedRooms.insert(u);
                        }
                        if (dir == ExitDirEnum::UP || dir == ExitDirEnum::DOWN) {
                            info.roomFlags[v] |= SubAreaFlagEnum::DISTANT;
                        }
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

    // Tarjan's SCC algorithm to locate components
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

    // Classify SCCs into sub-area clusters and assign topological flags
    uint32_t clusterCounter = 1;
    for (const auto &scc : sccs) {
        bool isMazeCluster = false;
        if (scc.size() == 1) {
            const RoomId v = scc.front();
            if (seedRooms.contains(v)) {
                isMazeCluster = true;
            }
        } else if (scc.size() >= 2) {
            for (const RoomId v : scc) {
                if (seedRooms.contains(v)) {
                    isMazeCluster = true;
                    break;
                }
            }
            if (scc.size() >= 3) {
                isMazeCluster = true;
            }
        }

        const uint32_t cId = clusterCounter++;
        for (const RoomId v : scc) {
            info.roomClusterId[v] = cId;
            RoomHandle rh = map.findRoomHandle(v);
            if (rh.exists() && rh.getSubAreaFlags().contains(SubAreaFlagEnum::LOCK_AUTO_CLASSIFY)) {
                info.roomFlags[v] = rh.getSubAreaFlags();
                continue;
            }

            SubAreaFlags flags = rh.exists() ? rh.getSubAreaFlags() : SubAreaFlags{};

            if (isMazeCluster) {
                flags |= SubAreaFlagEnum::MAZE;
            }

            // Degree / Linear pathway analysis
            const int inD = inDegree[v];
            const int outD = outDegree[v];

            if (inD == 1 && outD == 1) {
                flags |= SubAreaFlagEnum::TUNNEL;
            } else if (inD + outD >= 6) {
                flags |= SubAreaFlagEnum::HUB;
            }

            info.roomFlags[v] = flags;
        }
    }

    // Categorize connections & Chokepoint detection
    for (const RoomId v : allRooms) {
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
                    info.internalConnections.insert(key);
                } else {
                    info.boundaryExitConnections.insert(key);

                    // If exit connects two different clusters and is a critical bottleneck
                    if (outDegree[v] <= 2 && inDegree[u] <= 2) {
                        info.chokepointConnections.insert(key);
                        if (!rh.getSubAreaFlags().contains(SubAreaFlagEnum::LOCK_AUTO_CLASSIFY)) {
                            info.roomFlags[v] |= SubAreaFlagEnum::CHOKEPOINT;
                        }
                    }
                }
            }
        }
    }

    return info;
}
