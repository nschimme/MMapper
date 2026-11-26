// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "KnownRoomsTracker.h"

#include "../configuration/configuration.h"

#include <utility>

#include <QDir>
#include <QStandardPaths>

KnownRoomsTracker::KnownRoomsTracker(QObject *const parent, Locator locator)
    : QObject(parent)
    , m_locator{std::move(locator)}
{}

KnownRoomsTracker::~KnownRoomsTracker() = default;

void KnownRoomsTracker::afterChange(const uint64_t previousRevision, const bool request)
{
    if (previousRevision != getSnapshot().revision) {
        emit sig_changed();
    }
    if (request) {
        sendRequest();
    }
}

void KnownRoomsTracker::sendRequest()
{
    // The module is only asked for on demand; Core.Supports.Add is idempotent.
    emit sig_sendGmcp(
        GmcpMessage{GmcpMessageTypeEnum::CORE_SUPPORTS_ADD, GmcpJson{R"(["Room.Known 1"])"}});
    emit sig_sendGmcp(GmcpMessage{GmcpMessageTypeEnum::ROOM_KNOWN_LIST});
}

std::optional<KnownRoomsTracker::StoreLocation> KnownRoomsTracker::defaultLocation()
{
    const auto dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDirectory.isEmpty()) {
        return std::nullopt;
    }
    const auto &connection = getConfig().connection;
    return StoreLocation{QDir(dataDirectory).filePath("known-rooms"),
                         connection.remoteServerName.trimmed().toCaseFolded() + ":"
                             + QString::number(static_cast<unsigned int>(connection.remotePort))};
}

void KnownRoomsTracker::configureFromConnection()
{
    const auto location = m_locator ? m_locator() : std::nullopt;
    if (!location) {
        return;
    }
    const auto revision = getSnapshot().revision;
    m_known.configureCache(
        location->directory,
        location->server,
        [this]() { emit sig_changed(); },
        [this]() { emit sig_cacheMissing(); });
    afterChange(revision, false);
}

void KnownRoomsTracker::setEnabled(const bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    const auto revision = getSnapshot().revision;
    if (enabled) {
        configureFromConnection();
    }
    afterChange(revision, m_known.setEnabled(enabled));
}

void KnownRoomsTracker::onConnected()
{
    const auto revision = getSnapshot().revision;
    m_known.reset();
    if (m_enabled) {
        configureFromConnection();
    }
    afterChange(revision, false);
}

void KnownRoomsTracker::onDisconnected()
{
    const auto revision = getSnapshot().revision;
    m_known.reset();
    afterChange(revision, false);
}

void KnownRoomsTracker::slot_parseGmcpInput(const GmcpMessage &msg)
{
    const auto revision = getSnapshot().revision;
    const bool request = m_known.receive(msg);
    afterChange(revision, request);
}
