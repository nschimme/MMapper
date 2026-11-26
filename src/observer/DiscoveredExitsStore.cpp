// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "DiscoveredExitsStore.h"

#include <cstdint>
#include <utility>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {
constexpr uint32_t MAGIC = 0x58444D4D; // "MMDX" in little-endian byte order
constexpr uint32_t VERSION = 1;
constexpr uint32_t MAX_EXITS = 1'000'000;
constexpr qint64 BYTES_PER_EXIT = 5; // uint32 room + uint8 direction
constexpr qint64 MAX_FILE_BYTES = qint64{MAX_EXITS} * BYTES_PER_EXIT + 4096;
constexpr int FLUSH_DELAY_MS = 2000;

NODISCARD bool isStoredDirection(const uint8_t value)
{
    return value < NUM_EXITS_NESWUD; // N, E, S, W, U, D
}
} // namespace

DiscoveredExitsStore::DiscoveredExitsStore(QString directory, QString server)
    : m_directory{std::move(directory)}
    , m_server{std::move(server)}
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(FLUSH_DELAY_MS);
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this]() { flush(); });
}

DiscoveredExitsStore::~DiscoveredExitsStore()
{
    flush();
}

QString DiscoveredExitsStore::filename(const QString &character) const
{
    const auto key = m_server.toUtf8() + '\0' + character.toCaseFolded().toUtf8();
    const auto hash = QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
    return QDir(m_directory).filePath(QString::fromLatin1(hash) + ".exits");
}

std::optional<DiscoveredExits> DiscoveredExitsStore::load(const QString &character)
{
    const auto folded = character.toCaseFolded();
    if (const auto it = m_pending.find(folded); it != m_pending.end()) {
        const auto exits = it->second;
        m_pending.erase(it);
        writeFile(character, exits);
        return exits;
    }

    const auto path = filename(character);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt; // no file yet
    }

    const auto damaged = [&file, &path]() -> std::optional<DiscoveredExits> {
        file.close();
        // Keep the evidence; never let a later save replace it silently, and never overwrite an
        // earlier copy: a second corruption must not destroy what the first one left.
        QString aside = path + ".corrupt";
        for (int n = 1; QFile::exists(aside); ++n) {
            aside = path + ".corrupt." + QString::number(n);
        }
        if (!QFile::rename(path, aside)) {
            qWarning().noquote() << "[DiscoveredExits store] cannot set aside" << path;
        } else {
            qWarning().noquote() << "[DiscoveredExits store] damaged file kept as" << aside;
        }
        return std::nullopt;
    };

    if (file.size() > MAX_FILE_BYTES) {
        return damaged();
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
        || server != m_server.toUtf8() || who != folded.toUtf8() || count > MAX_EXITS) {
        return damaged();
    }
    if (bytes.size() - in.device()->pos() != qint64{count} * BYTES_PER_EXIT) {
        return damaged();
    }

    DiscoveredExits exits;
    std::optional<DiscoveredExit> previous;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t room = 0;
        uint8_t dir = 0;
        in >> room >> dir;
        if (in.status() != QDataStream::Ok || room == 0 || !isStoredDirection(dir)) {
            return damaged();
        }
        const DiscoveredExit exit{ServerRoomId{room}, static_cast<ExitDirEnum>(dir)};
        // Strictly increasing also rejects duplicates.
        if (previous && !(*previous < exit)) {
            return damaged();
        }
        previous = exit;
        exits.insert(exit);
    }
    return exits;
}

void DiscoveredExitsStore::save(const QString &character,
                                const DiscoveredExits &exits,
                                const bool immediate)
{
    m_pending.insert_or_assign(character.toCaseFolded(), exits);
    if (immediate) {
        flush();
        return;
    }
    if (!m_timer.isActive()) {
        m_timer.start();
    }
}

void DiscoveredExitsStore::flush()
{
    m_timer.stop();
    auto pending = std::exchange(m_pending, {});
    for (const auto &[character, exits] : pending) {
        writeFile(character, exits);
    }
}

void DiscoveredExitsStore::writeFile(const QString &character, const DiscoveredExits &exits) const
{
    if (!QDir().mkpath(m_directory)) {
        qWarning() << "[DiscoveredExits store] Cannot create the directory.";
        return;
    }

    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out.setVersion(QDataStream::Qt_6_0);
        out << MAGIC << VERSION << m_server.toUtf8() << character.toCaseFolded().toUtf8()
            << static_cast<uint32_t>(exits.size());
        for (const auto &exit : exits) { // std::set iterates in sorted order
            out << exit.room.asUint32() << static_cast<uint8_t>(enums::to_underlying(exit.dir));
        }
    }

    QSaveFile file(filename(character));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        qWarning().noquote() << "[DiscoveredExits store] Cannot save:" << file.errorString();
    }
}
