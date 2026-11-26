// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "KnownRoomsStore.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {
constexpr uint32_t MAGIC = 0x524B4D4D; // "MMKR" in little-endian byte order
constexpr uint32_t VERSION = 1;
constexpr uint32_t MAX_ROOMS = 2'000'000;
constexpr qint64 MAX_FILE_BYTES = qint64{MAX_ROOMS} * 4 + 4096;
constexpr int FLUSH_DELAY_MS = 2000;

void report(const QString &error)
{
    qWarning().noquote() << "[KnownRooms store]" << error;
}
} // namespace

KnownRoomsStore::KnownRoomsStore(QString directory, QString server)
    : m_directory{std::move(directory)}
    , m_server{std::move(server)}
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(FLUSH_DELAY_MS);
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this]() { flush(); });
}

KnownRoomsStore::~KnownRoomsStore()
{
    // Normal shutdown writes the last coalesced updates.
    flush();
}

QString KnownRoomsStore::filename(const QString &character) const
{
    // Neither server names nor character names can introduce path components.
    const auto key = m_server.toUtf8() + '\0' + character.toCaseFolded().toUtf8();
    const auto hash = QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
    return QDir(m_directory).filePath(QString::fromLatin1(hash) + ".bin");
}

std::optional<KnownRoomsStore::Rooms> KnownRoomsStore::load(const QString &character)
{
    const auto folded = character.toCaseFolded();
    if (const auto it = m_pending.find(folded); it != m_pending.end()) {
        const auto rooms = it->second;
        m_pending.erase(it);
        writeFile(character, rooms);
        return rooms;
    }

    QFile file(filename(character));
    if (!file.open(QIODevice::ReadOnly) || file.size() > MAX_FILE_BYTES) {
        return std::nullopt;
    }
    const auto bytes = file.readAll();

    QDataStream in(bytes);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);
    uint32_t magic = 0;
    uint32_t version = 0;
    QByteArray server;
    QByteArray who;
    uint32_t count = 0;
    in >> magic >> version >> server >> who >> count;
    if (in.status() != QDataStream::Ok || magic != MAGIC || version != VERSION
        || server != m_server.toUtf8() || who != folded.toUtf8() || count > MAX_ROOMS) {
        return std::nullopt;
    }
    // The payload must be exactly the advertised IDs: truncation and padding are both damage.
    const qint64 expected = qint64{count} * 4;
    if (bytes.size() - in.device()->pos() != expected) {
        return std::nullopt;
    }

    Rooms rooms;
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id = 0;
        in >> id;
        // IDs are stored strictly increasing, which also rejects zero and duplicates.
        if (in.status() != QDataStream::Ok || id <= previous) {
            return std::nullopt;
        }
        previous = id;
        rooms.insert(ServerRoomId{id});
    }
    return rooms;
}

void KnownRoomsStore::save(const QString &character, const Rooms &rooms, const bool immediate)
{
    const auto folded = character.toCaseFolded();
    m_pending.insert_or_assign(folded, rooms);
    if (immediate) {
        flush();
        return;
    }
    // Keep the original deadline during continuous updates so writes cannot starve.
    if (!m_timer.isActive()) {
        m_timer.start();
    }
}

void KnownRoomsStore::flush()
{
    m_timer.stop();
    auto pending = std::exchange(m_pending, {});
    for (const auto &[character, rooms] : pending) {
        writeFile(character, rooms);
    }
}

void KnownRoomsStore::writeFile(const QString &character, const Rooms &rooms) const
{
    if (!QDir().mkpath(m_directory)) {
        report("Cannot create the known-room directory.");
        return;
    }

    std::vector<uint32_t> ids;
    ids.reserve(rooms.size());
    rooms.for_each([&ids](const ServerRoomId id) { ids.push_back(id.asUint32()); });
    std::sort(ids.begin(), ids.end());

    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out.setVersion(QDataStream::Qt_6_0);
        out << MAGIC << VERSION << m_server.toUtf8() << character.toCaseFolded().toUtf8()
            << static_cast<uint32_t>(ids.size());
        for (const uint32_t id : ids) {
            out << id;
        }
    }

    QSaveFile file(filename(character));
    // QSaveFile defaults to atomic replacement; do not enable direct-write fallback.
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        report("Cannot save the known-room file: " + file.errorString());
    }
}
