#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "../global/Timer.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/infomark.h"
#include "../map/room.h" // For RoomHandle
#include "../map/roomid.h"
#include "../opengl/Font.h"
#include "Connections.h" // May not be strictly needed here but was in original
#include "IMapBatchesFinisher.h"
#include "Infomarks.h"       // May not be strictly needed here
#include "MapBatches.h"      // Forward declaration or include needed if Batches uses LayerBatchData fully defined in .cpp
#include "MapCanvasData.h"   // For UniqueMesh etc. if Batches uses it.
#include "RoadIndex.h"       // For RoadIndexMaskEnum

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Includes for RoomGeometry members (were previously in a different place)
#include <string>
// "../map/roomflags.h" is not needed; RoomLoadFlags and RoomMobFlags are available via "../map/room.h"
#include "../map/enums.h"     // For RoomLightType, RoomRidableType, RoomSundeathType, RoomTerrainEnum etc.
#include "../map/ExitFlags.h" // For ExitFlags
#include "../map/DoorFlags.h" // For DoorFlags


class OpenGL; // Forward declaration
class QOpenGLTexture; // Forward declaration
struct MapCanvasTextures; // Forward declaration

using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>;

// Refined RoomExitGeometry
struct RoomExitGeometry {
    ExitFlags exitFlags;
    DoorFlags doorFlags;
    bool outIsEmpty;
    bool hasIncomingStream;

    bool operator==(const RoomExitGeometry& other) const {
        return exitFlags.value() == other.exitFlags.value() &&
               doorFlags.value() == other.doorFlags.value() &&
               outIsEmpty == other.outIsEmpty &&
               hasIncomingStream == other.hasIncomingStream;
    }
};

// Refined RoomGeometry
struct RoomGeometry final
{
    RoomLoadFlags loadFlags;
    RoomMobFlags mobFlags;
    RoomLightType lightType;
    RoomRidableType ridableType;
    RoomSundeathType sundeathType;
    RoomTerrainEnum terrainType;
    RoadIndexMaskEnum roadIndex;

    EnumIndexedArray<RoomExitGeometry, ExitDirEnum, NUM_EXIT_DIRECTIONS_NESWUD> exits;

    bool operator==(const RoomGeometry &other) const
    {
        if (!(loadFlags.value() == other.loadFlags.value() &&
              mobFlags.value() == other.mobFlags.value() &&
              lightType == other.lightType &&
              ridableType == other.ridableType &&
              sundeathType == other.sundeathType &&
              terrainType == other.terrainType &&
              roadIndex == other.roadIndex)) {
            return false;
        }
        for (size_t i = 0; i < exits.size(); ++i) {
             // Assuming ExitDirEnum can be cast to size_t for direct indexing if needed,
             // or EnumIndexedArray handles correct iteration/access.
             // For direct comparison using operator[] with Enum, ensure EnumIndexedArray supports it well.
            if (!(exits[static_cast<ExitDirEnum>(i)] == other.exits[static_cast<ExitDirEnum>(i)])) {
                return false;
            }
        }
        return true;
    }
};

namespace std {
// Hash for RoomExitGeometry
template <>
struct hash<RoomExitGeometry> {
    std::size_t operator()(const RoomExitGeometry &exit_geom) const {
        size_t seed = 0;
        auto hash_combine = [&](size_t& current_seed, auto const& val) {
            current_seed ^= std::hash<decltype(val)>()(val) + 0x9e3779b9 + (current_seed << 6) + (current_seed >> 2);
        };
        hash_combine(seed, exit_geom.exitFlags.value());
        hash_combine(seed, exit_geom.doorFlags.value());
        hash_combine(seed, exit_geom.outIsEmpty);
        hash_combine(seed, exit_geom.hasIncomingStream);
        return seed;
    }
};

// Hash for RoomGeometry
template <>
struct hash<RoomGeometry> {
    std::size_t operator()(const RoomGeometry &geom) const
    {
        size_t seed = 0;
        auto hash_combine = [&](size_t& current_seed, auto const& val) {
            current_seed ^= std::hash<decltype(val)>()(val) + 0x9e3779b9 + (current_seed << 6) + (current_seed >> 2);
        };

        hash_combine(seed, geom.loadFlags.value());
        hash_combine(seed, geom.mobFlags.value());
        hash_combine(seed, static_cast<int>(geom.lightType));
        hash_combine(seed, static_cast<int>(geom.ridableType));
        hash_combine(seed, static_cast<int>(geom.sundeathType));
        hash_combine(seed, static_cast<int>(geom.terrainType));
        hash_combine(seed, static_cast<int>(geom.roadIndex));

        for (const auto& exit_geom_val : geom.exits) { // EnumIndexedArray should be iterable
            hash_combine(seed, std::hash<RoomExitGeometry>()(exit_geom_val));
        }
        return seed;
    }
};
} // namespace std


// Forward declare LayerBatchData if its full definition is in .cpp
// This is needed if Batches struct (below) uses it by value or reference.
// However, Batches struct is not defined in this file in the current known structure.
// It's in MapCanvasData.h or MapBatches.h.

// RemeshCookie, Batches, generateMapDataFinisher, finish are typically defined after
// all necessary structs they depend on or with forward declarations.
// The original file structure suggests Batches is not in this file.
// Let's keep the original RemeshCookie and other declarations that were present.

struct NODISCARD RemeshCookie final
{
private:
    std::optional<FutureSharedMapBatchFinisher> m_opt_future;
    bool m_ignored = false;

private:
    void reset() { *this = RemeshCookie{}; }

public:
    void setIgnored()
    {
        m_ignored = true;
    }

public:
    NODISCARD bool isPending() const { return m_opt_future.has_value(); }
    NODISCARD bool isReady() const
    {
        const FutureSharedMapBatchFinisher &future = m_opt_future.value();
        return future.valid()
               && future.wait_for(std::chrono::nanoseconds(0)) != std::future_status::timeout;
    }

private:
    static void reportException()
    {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception &ex) {
            qWarning() << "Exception: " << ex.what();
        } catch (...) {
            qWarning() << "Unknown exception";
        }
    }

public:
    NODISCARD SharedMapBatchFinisher get()
    {
        DECL_TIMER(t, __FUNCTION__);
        FutureSharedMapBatchFinisher &future = m_opt_future.value();
        SharedMapBatchFinisher pFinisher;
        try {
            pFinisher = future.get();
        } catch (...) {
            reportException();
            pFinisher.reset();
        }
        if (m_ignored) {
            pFinisher.reset();
        }
        reset();
        return pFinisher;
    }

public:
    void set(FutureSharedMapBatchFinisher future)
    {
        if (m_opt_future && !m_ignored) {
            throw std::runtime_error("replaced existing future");
        }
        m_opt_future.emplace(std::move(future));
        m_ignored = false;
    }
};


// Batches struct is defined in MapCanvasData.h or MapBatches.h, not here.
// Assuming it's correctly forward-declared or its header included if needed by functions below.

NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures, const Map &map);

extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches, // MapBatches comes from MapBatches.h
                   OpenGL &gl,
                   GLFont &font);

// LayerBatchData struct definition is in MapCanvasRoomDrawer.cpp
// LayerBatchBuilder class definition is in MapCanvasRoomDrawer.cpp
// visitRoom and visitRooms static functions are in MapCanvasRoomDrawer.cpp
// Other helper static functions like getRoomTerrainAndTrail are in MapCanvasRoomDrawer.cpp
// IRoomVisitorCallbacks is an interface class defined in MapCanvasRoomDrawer.cpp (or should be in .h if used externally)
// For now, assuming IRoomVisitorCallbacks is used only by LayerBatchBuilder within the .cpp.
// If it needs to be exposed, its definition should be here.
// The original file had IRoomVisitorCallbacks in the .cpp, so this should be fine.

// Note: QColor and glm includes were moved up. Original file had them after some project includes.
// For glm::vec2 used in original RoomGeometry, that include is present.
// QColor is not used in the refined RoomGeometry directly.
// Ensure all types used by kept members of RoomGeometry (RoomLoadFlags, etc.) are included.
// RoadIndex.h for RoadIndexMaskEnum is included.
// enums.h for various RoomXXXEnums is included.
// roomflags.h for RoomLoadFlags, RoomMobFlags is included.
// ExitFlags.h and DoorFlags.h are included.
// Array.h for EnumIndexedArray is included.
// ExitDirection.h for ExitDirEnum and NUM_EXIT_DIRECTIONS_NESWUD is included.
// std::string is included.
// This seems complete for RoomGeometry and RoomExitGeometry.
