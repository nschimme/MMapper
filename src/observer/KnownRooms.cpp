// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "KnownRooms.h"

#include "../proxy/GmcpMessage.h"
#include "KnownRoomsStore.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace {
NODISCARD std::optional<ServerRoomId> roomId(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 1.0
        || number > static_cast<double>(std::numeric_limits<uint32_t>::max())
        || std::floor(number) < number) {
        return std::nullopt;
    }
    return ServerRoomId{static_cast<uint32_t>(number)};
}
} // namespace

KnownRooms::KnownRooms() = default;
KnownRooms::~KnownRooms() = default;

void KnownRooms::configureCache(const QString &directory,
                                const QString &server,
                                std::function<void()> changed,
                                std::function<void()> missing)
{
    if (m_cache && m_cache->matches(directory, server)) {
        return;
    }
    reset();
    // Destroying the old store flushes its pending writes before the new server is used.
    m_cache.reset();
    m_cacheChanged = std::move(changed);
    m_cacheMissing = std::move(missing);
    m_cache = std::make_unique<KnownRoomsStore>(directory, server);
}

void KnownRooms::loadCache()
{
    if (!m_cache || m_character.isEmpty()) {
        return;
    }
    // Reads are small and synchronous, so a result can never arrive for a stale character.
    if (const auto rooms = m_cache->load(m_character)) {
        restoreCache(m_character, m_snapshot.generation, *rooms);
    } else if (!m_snapshot.listComplete && m_enabled && !m_cacheWarningIssued) {
        m_cacheWarningIssued = true;
        if (m_cacheMissing) {
            m_cacheMissing();
        }
    }
}

void KnownRooms::saveCache(const bool immediate)
{
    // Additions are always safe to persist; removals only ever arrive with a completed list,
    // which replaces the whole set first. A good cache is never replaced by a smaller one.
    if (m_cache && !m_character.isEmpty()
        && (m_snapshot.listComplete || !m_snapshot.rooms.empty())) {
        m_cache->save(m_character, m_snapshot.rooms, immediate);
    }
}

void KnownRooms::restoreCache(const QString &character,
                              const uint64_t generation,
                              const ImmUnorderedSet<ServerRoomId> &rooms)
{
    if (character != m_character || generation != m_snapshot.generation || m_authoritative) {
        return; // Cached history never overrides a newer character or server truth.
    }
    // Additions only: anything already received stays, and the cache supplies the rest.
    rooms.for_each([this](const ServerRoomId id) { m_snapshot.rooms.insert(id); });
    ++m_snapshot.revision;
    if (m_cacheChanged) {
        m_cacheChanged();
    }
}

void KnownRooms::invalidate()
{
    m_snapshot.rooms = ImmUnorderedSet<ServerRoomId>{};
    m_snapshot.listComplete = false;
    m_authoritative = false;
    m_liveKnown = ImmUnorderedSet<ServerRoomId>{};
    ++m_snapshot.revision;
    m_snapshot.generation = m_snapshot.revision;
}

void KnownRooms::reset()
{
    invalidate();
    m_pending = ImmUnorderedSet<ServerRoomId>{};
    m_character.clear();
    m_cacheWarningIssued = false;
    m_receiving = false;
    m_discard = false;
    m_refresh = false;
}

bool KnownRooms::requestList()
{
    // GMCP has no request IDs. Drain an old response before requesting a replacement.
    if (m_receiving) {
        m_discard = true;
        m_refresh = m_enabled && !m_character.isEmpty();
        return false;
    }
    if (!m_enabled || m_character.isEmpty()) {
        return false;
    }
    m_pending = ImmUnorderedSet<ServerRoomId>{};
    m_receiving = true;
    m_discard = false;
    m_refresh = false;
    return true;
}

bool KnownRooms::setEnabled(const bool enabled)
{
    if (m_enabled == enabled) {
        return false;
    }
    m_enabled = enabled;
    if (m_cache) {
        // Disabling the overlay does not discard an in-flight refresh or live discoveries.
        if (!enabled || (m_receiving && !m_discard)) {
            return false;
        }
        return requestList();
    }
    invalidate();
    if (!enabled) {
        m_discard = true;
        m_refresh = false;
        return false;
    }
    return requestList();
}

bool KnownRooms::receive(const GmcpMessage &message)
{
    if (message.isCoreGoodbye()) {
        invalidate();
        m_character.clear();
        m_discard = true;
        m_refresh = false;
        return false;
    }
    if (message.isCharName()) {
        const auto &doc = message.getJsonDocument();
        const auto obj = doc ? doc->getObject() : std::nullopt;
        const auto name = obj ? obj->getString("name") : std::nullopt;
        if (!name || name->isEmpty()) {
            invalidate();
            m_character.clear();
            m_discard = true;
            m_refresh = false;
            return false;
        }
        if (*name != m_character) {
            invalidate();
            m_character = *name;
            m_cacheWarningIssued = false;
            loadCache();
            return requestList();
        }
        if (!m_snapshot.listComplete && !m_receiving) {
            return requestList();
        }
        return false;
    }
    if (message.isRoomKnownUpdated()) {
        // The server's history changed in a way we cannot patch. Keep the rooms we hold (they
        // remain valid additions) but they are provisional until the refreshed list ends.
        m_authoritative = false;
        m_snapshot.listComplete = false;
        ++m_snapshot.revision;
        return requestList();
    }
    if (!message.isRoomKnownList() && !message.isRoomKnownAdd()) {
        return false;
    }
    // Still drain a response when the option is disabled during a transfer.
    if (!m_enabled && !m_cache && (!message.isRoomKnownList() || !m_receiving)) {
        return false;
    }

    const auto &json = message.getJson();
    if (!json) {
        if (message.isRoomKnownList() && m_receiving) {
            m_discard = true;
        }
        return false;
    }
    // Wrapping permits strict parsing of scalar JSON on Qt versions where QJsonDocument
    // only accepts arrays and objects, including the protocol's null/false sentinels.
    const auto wrapper = QJsonDocument::fromJson("[" + json->toQByteArray() + "]");
    if (!wrapper.isArray() || wrapper.array().size() != 1) {
        if (message.isRoomKnownList() && m_receiving) {
            m_discard = true;
        }
        return false;
    }
    const auto value = wrapper.array().at(0);
    if (message.isRoomKnownAdd()) {
        if (m_enabled || m_cache) {
            if (const auto id = roomId(value)) {
                m_liveKnown.insert(*id);
                if (m_receiving && !m_discard) {
                    m_pending.insert(*id);
                }
                if (!m_snapshot.rooms.contains(*id)) {
                    m_snapshot.rooms.insert(*id);
                    ++m_snapshot.revision;
                    saveCache();
                }
            }
        }
        return false;
    }
    if (!m_receiving) {
        return false;
    }
    if (value.isNull() || (value.isBool() && !value.toBool())) {
        m_receiving = false;
        if (value.isNull() && !m_discard && (m_enabled || m_cache)) {
            m_snapshot.rooms = std::move(m_pending);
            m_snapshot.listComplete = true;
            m_authoritative = true;
            ++m_snapshot.revision;
            saveCache(true);
        }
        m_pending = ImmUnorderedSet<ServerRoomId>{};
        if (m_refresh) {
            return requestList();
        }
        return false;
    }
    if (!value.isArray()) {
        m_discard = true;
        return false;
    }
    if (!m_discard) {
        // Validate the whole chunk first: a damaged chunk is ignored entirely and cancels the
        // final replacement, but rooms from earlier valid chunks stay known.
        std::vector<ServerRoomId> ids;
        for (const auto entry : value.toArray()) {
            const auto id = roomId(entry);
            if (!id) {
                m_discard = true;
                break;
            }
            ids.push_back(*id);
        }
        if (!m_discard) {
            bool added = false;
            for (const auto id : ids) {
                m_pending.insert(id);
                if (!m_snapshot.rooms.contains(id)) {
                    m_snapshot.rooms.insert(id);
                    added = true;
                }
            }
            if (added) {
                ++m_snapshot.revision;
                saveCache();
            }
        }
    }
    return false;
}
