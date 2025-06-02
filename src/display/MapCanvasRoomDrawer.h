#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "../global/Timer.h" // Still used by Batches::FlashState (indirectly via getTime())
#include "../map/ExitDirection.h" // Potentially used by other non-removed parts, keep for now
#include "../map/coordinate.h" // Potentially used
#include "../map/infomark.h" // For BatchedInfomarksMeshes
#include "../map/room.h" // Potentially used
#include "../map/roomid.h" // Potentially used
#include "../opengl/Font.h" // Potentially used by other non-removed parts
#include "Connections.h" // For BatchedConnectionMeshes (used in MapBatches)
// #include "IMapBatchesFinisher.h" // REMOVE - Part of old system
#include "Infomarks.h" // For BatchedInfomarksMeshes
#include "MapBatches.h" // Definition of MapBatches, which is still used by Batches struct (for m_areaMapBatches removal, but MapBatches itself is a return type)
#include "MapCanvasData.h" // For various canvas related data, keep for now
#include "RoadIndex.h"

#include <map>
#include <optional>
#include <set> // Added for std::set
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QColor>
#include <QtCore>

class OpenGL;
class QOpenGLTexture;
struct MapCanvasTextures;

// RoomVector and LayerToRooms are likely specific to the old drawing logic or general utilities.
// Keep them for now unless they are confirmed to be exclusively for the removed parts.
using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>;

// RemeshCookie is removed as AsyncMapAreaManager handles its own task management.

// Batches struct will be simplified. It will no longer hold area-specific map batches or cookies.
// It will retain infomarksMeshes and pendingUpdateFlashState.
struct NODISCARD Batches final
{
    // Area-specific members REMOVED:
    // std::map<std::string, RemeshCookie> m_areaRemeshCookies;
    // std::map<std::string, MapBatches> m_areaMapBatches;
    // std::set<std::string> m_areasMarkedForCatchUp;

    // Infomarks are considered global for now
    std::optional<BatchedInfomarksMeshes> infomarksMeshes;

    struct NODISCARD FlashState final
    {
    private:
        std::chrono::steady_clock::time_point lastChange = getTime();
        bool on = false;

        // getTime needs to be static or moved if FlashState is to be self-contained
        // For now, assume getTime() is accessible, perhaps a global helper or static in outer scope.
        // If not, this needs adjustment. Let's assume it's findable for now.
        static std::chrono::steady_clock::time_point getTime() 
        {
            return std::chrono::steady_clock::now();
        }

    public:
        NODISCARD bool tick()
        {
            const auto flash_interval = std::chrono::milliseconds(100);
            const auto now = getTime();
            if (now >= lastChange + flash_interval) {
                lastChange = now;
                on = !on;
            }
            return on;
        }
    };
    FlashState pendingUpdateFlashState;

    Batches() = default;
    ~Batches() = default;
    DEFAULT_MOVES_DELETE_COPIES(Batches);

    void resetExistingMeshesButKeepPendingRemesh()
    {
        // m_areaMapBatches.clear(); // Removed
        infomarksMeshes.reset();
        // Area-specific cookies and catch-up list are gone.
    }

    void ignorePendingRemesh()
    {
        // No area-specific cookies to ignore.
        // If there were global cookies for other things, they'd be handled here.
    }

    void resetExistingMeshesAndIgnorePendingRemesh()
    {
        resetExistingMeshesButKeepPendingRemesh();
        ignorePendingRemesh();
        // m_areasMarkedForCatchUp.clear(); // Removed
    }
};

// generateMapDataFinisher is REMOVED - Its functionality is replaced by AsyncMapAreaManager.
// extern void finish(...) is REMOVED - Its functionality is replaced by AsyncMapAreaManager.
