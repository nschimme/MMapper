#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/ImmUnorderedSet.h"
#include "../global/macros.h"
#include "../map/roomid.h"

#include <map>
#include <optional>

#include <QString>
#include <QTimer>

// Per-server, per-character persistence for the set of known room IDs.
//
// The file is a small binary record (header, identity, sorted uint32 IDs), so reads and
// writes are cheap enough for the GUI thread: no worker threads and no futures.
// Writes are coalesced behind a short timer and always flushed on destruction.
// The caller supplies Qt's standard application-data path; tests use a temporary directory.
class NODISCARD KnownRoomsStore final
{
public:
    using Rooms = ImmUnorderedSet<ServerRoomId>;

private:
    QString m_directory;
    QString m_server;
    QTimer m_timer;
    // Keyed by case-folded character so both spellings share one pending write.
    std::map<QString, Rooms> m_pending;

    void writeFile(const QString &character, const Rooms &rooms) const;
    NODISCARD QString filename(const QString &character) const;

public:
    KnownRoomsStore(QString directory, QString server);
    ~KnownRoomsStore();
    DELETE_CTORS_AND_ASSIGN_OPS(KnownRoomsStore);

    // Returns nothing for a missing, damaged, or foreign file. A newer pending write for the
    // same character is flushed first so a read never returns older data than we hold.
    NODISCARD std::optional<Rooms> load(const QString &character);
    void save(const QString &character, const Rooms &rooms, bool immediate = false);
    void flush();

    NODISCARD bool hasPendingWrites() const { return !m_pending.empty(); }
    NODISCARD bool matches(const QString &directory, const QString &server) const
    {
        return m_directory == directory && m_server == server;
    }
};
