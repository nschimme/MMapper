// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "KnownRoomsTracker.h"

#include "../configuration/configuration.h"
#include "../global/JsonArray.h"
#include "../global/JsonObj.h"
#include "DiscoveredExitsStore.h"

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
    if (!m_exitStore || !m_exitStore->matches(location->directory, location->server)) {
        m_exitStore = std::make_unique<DiscoveredExitsStore>(location->directory, location->server);
        m_exitCharacter.clear(); // force a reload for whoever is logged in
    }
    followCharacter();
    afterChange(revision, false);
}

void KnownRoomsTracker::followCharacter()
{
    const QString &character = m_known.getCharacter();
    if (character == m_exitCharacter) {
        return;
    }
    // Exits are isolated per character: never carry one character's finds to another.
    m_exitCharacter = character;
    m_exits.clear();
    if (m_exitStore && !character.isEmpty()) {
        if (const auto saved = m_exitStore->load(character)) {
            m_exits = *saved;
        }
    }
    ++m_exitsRevision;
    emit sig_characterChanged();
    emit sig_exitsChanged();
}

void KnownRoomsTracker::recordHiddenExits(const GmcpMessage &msg)
{
    if (!m_enabled || !m_exitStore || m_exitCharacter.isEmpty()) {
        return;
    }
    const auto &doc = msg.getJsonDocument();
    const auto obj = doc ? doc->getObject() : std::nullopt;
    if (!obj) {
        return;
    }
    // Same fields as MumeXmlParser::parseGmcpRoomInfo: "id", and per-direction "exits"
    // entries whose "flags" may contain "hidden" once the server shows the exit to the player.
    const auto id = obj->getInt("id");
    const auto exits = obj->getObject("exits");
    if (!id || *id < 1 || !exits) {
        return;
    }
    const ServerRoomId room{static_cast<uint32_t>(*id)};
    bool added = false;
    for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
        const char key[2] = {lowercaseDirection(dir)[0], '\0'};
        const auto exit = exits->getObject(QString::fromLatin1(key));
        const auto flags = exit ? exit->getArray("flags") : std::nullopt;
        if (!flags) {
            continue;
        }
        for (const JsonValue &flag : *flags) {
            if (const auto name = flag.getString(); name && *name == "hidden") {
                added |= m_exits.insert(DiscoveredExit{room, dir}).second;
            }
        }
    }
    if (added) {
        ++m_exitsRevision;
        m_exitStore->save(m_exitCharacter, m_exits);
        emit sig_exitsChanged();
    }
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
    followCharacter();
    if (m_enabled) {
        configureFromConnection();
    }
    afterChange(revision, false);
}

void KnownRoomsTracker::onDisconnected()
{
    const auto revision = getSnapshot().revision;
    m_known.reset();
    if (m_exitStore) {
        m_exitStore->flush();
    }
    followCharacter();
    afterChange(revision, false);
}

void KnownRoomsTracker::slot_parseGmcpInput(const GmcpMessage &msg)
{
    const auto revision = getSnapshot().revision;
    const bool request = m_known.receive(msg);
    followCharacter();
    if (msg.isRoomInfo()) {
        recordHiddenExits(msg);
    }
    afterChange(revision, request);
}
