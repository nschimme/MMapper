#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "../global/Timer.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../opengl/Font.h"
#include "Connections.h"
#include "IMapBatchesFinisher.h"
#include "Infomarks.h"
#include "MapBatches.h"
#include "MapCanvasData.h"
#include "RoadIndex.h"

#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QColor>
#include <QtCore>

class OpenGL;
class QOpenGLTexture;
struct MapCanvasTextures;

using RoomVector = std::vector<RoomHandle>;
using LayerToRooms = std::map<int, RoomVector>;

struct NODISCARD RemeshCookie final
{
public: // Made public for access from MapCanvas code that creates/manages these
    RoomArea m_area;
    bool m_needsFollowUp = false;

private:
    std::optional<FutureSharedMapBatchFinisher> m_future_opt; // Renamed for clarity from m_opt_future
    bool m_ignored = false;

private:
    // NOTE: If you think you want to make this public, then you're really looking for setIgnored().
    void reset_internal() { // Renamed to avoid conflict if a public reset() is added later
        m_future_opt.reset();
        // m_area and m_needsFollowUp are not reset here, they persist if cookie is reused.
        // m_ignored is also not reset here, it's controlled by setIgnored or set a new future.
    }

public:
    // This means you've decided to load a completely new map, so you're not interested
    // in the results. It SHOULD NOT be used just because you got another mesh request.
    void setIgnored() { m_ignored = true; }
    NODISCARD bool isIgnored() const { return m_ignored; }


public:
    NODISCARD bool isPending() const { return m_future_opt.has_value(); }

    // Don't call this unless isPending() is true.
    // returns true if get() will return without blocking
    NODISCARD bool isReady() const
    {
        if (!isPending()) return false; // Check ensures m_future_opt has value
        const FutureSharedMapBatchFinisher &future = m_future_opt.value();
        // No assert needed due to check above.
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; // Using seconds(0) as per original intent
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
    // Don't call this unless isPending() is true.
    // NOTE: This can throw an exception thrown by the async function!
    NODISCARD SharedMapBatchFinisher get()
    {
        DECL_TIMER(t, __FUNCTION__);
        if (!isPending()) return nullptr; // Or throw

        FutureSharedMapBatchFinisher &future = m_future_opt.value();
        SharedMapBatchFinisher pFinisher;
        try {
            pFinisher = future.get(); // This consumes the future state
        } catch (...) {
            reportException();
            pFinisher.reset(); // Ensure null on exception
        }

        bool was_ignored = m_ignored;
        reset_internal(); // Resets m_future_opt

        if (was_ignored) { // Check after reset_internal might be too late if reset_internal changes m_ignored
            return nullptr;
        }
        return pFinisher;
    }

public:
    // Don't call this if isPending() is true, unless you called set_ignored().
    void set(FutureSharedMapBatchFinisher future)
    {
        if (m_future_opt.has_value() && !m_ignored) {
            // If this happens, you should wait until the old one is finished first,
            // or we're going to end up with tons of in-flight future meshes.
            // For now, let's log and overwrite, or rely on MapCanvas logic to manage this.
            qWarning() << "RemeshCookie::set called on a pending (and not ignored) cookie. Overwriting.";
            // Consider if an exception is more appropriate: throw std::runtime_error("replaced existing future");
        }

        m_future_opt.emplace(std::move(future));
        m_ignored = false; // Setting a new future means it's not ignored by default
        m_needsFollowUp = false; // Typically reset when a new future is set explicitly
    }

    // Explicit public reset to clear the future, matching original intent from mapcanvas.h's version
    void reset() {
        reset_internal();
        m_ignored = false;
        // m_area and m_needsFollowUp are not reset by this public reset.
    }
};

// #include <string> // No longer needed for map keys here, RoomArea is used. std::string might be used elsewhere.

struct NODISCARD Batches final
{
    // For per-area remeshing
    std::vector<RemeshCookie> m_areaRemeshCookies; // Changed from map to vector
    std::map<RoomArea, MapBatches> m_areaMapBatches;

    // Infomarks are considered global for now
    std::optional<BatchedInfomarksMeshes> infomarksMeshes;

    struct NODISCARD FlashState final
    {
    private:
        std::chrono::steady_clock::time_point lastChange = getTime();
        bool on = false;

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

    NODISCARD bool isPending() const
    {
        for (const auto &cookie : m_areaRemeshCookies) { // Iterate vector
            if (cookie.isPending()) {
                return true;
            }
        }
        return false;
    }

    void resetExistingMeshesButKeepPendingRemesh()
    {
        qInfo() << "Batches: Resetting existing meshes but keeping pending remesh tasks.";
        m_areaMapBatches.clear();
        infomarksMeshes.reset(); // Assuming infomarks are global and should be cleared here too

        // Iterate and remove cookies that are not pending.
        m_areaRemeshCookies.erase(
            std::remove_if(m_areaRemeshCookies.begin(), m_areaRemeshCookies.end(),
                           [](const RemeshCookie& c) { return !c.isPending(); }),
            m_areaRemeshCookies.end());
    }

    void ignorePendingRemesh() // This now iterates the vector
    {
        for (auto &cookie : m_areaRemeshCookies) {
            cookie.setIgnored();
        }
    }

    void resetExistingMeshesAndIgnorePendingRemesh()
    {
        qInfo() << "Batches: Resetting existing meshes and ignoring all pending remesh tasks.";
        m_areaMapBatches.clear();
        infomarksMeshes.reset(); // Clear global infomarks

        // Unlike original, we don't call ignorePendingRemesh() on each cookie,
        // we just clear the whole vector as per subtask instructions.
        m_areaRemeshCookies.clear();
    }
};

NODISCARD FutureSharedMapBatchFinisher
generateMapDataFinisher(const mctp::MapCanvasTexturesProxy &textures,
                        const Map &map,
                        std::optional<RoomArea> areaKey = std::nullopt);

extern void finish(const IMapBatchesFinisher &finisher,
                   std::optional<MapBatches> &batches,
                   OpenGL &gl,
                   GLFont &font);
