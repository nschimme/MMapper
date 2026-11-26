#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../map/DiscoveredExit.h"

#include <map>
#include <optional>

#include <QString>
#include <QTimer>

// Per-server, per-character persistence for the secret exits a character has found.
//
// This is client-only, irreplaceable data (the server cannot re-send it), unlike the known-room
// history, so it lives in its own file: a completed Room.Known.List can never touch it, and a
// damaged file is moved aside rather than overwritten by a later save.
// Same shape as KnownRoomsStore: small binary record, atomic QSaveFile, debounced writes,
// always flushed on destruction, and no threads.
class NODISCARD DiscoveredExitsStore final
{
private:
    QString m_directory;
    QString m_server;
    QTimer m_timer;
    std::map<QString, DiscoveredExits> m_pending;

    void writeFile(const QString &character, const DiscoveredExits &exits) const;
    NODISCARD QString filename(const QString &character) const;

public:
    DiscoveredExitsStore(QString directory, QString server);
    ~DiscoveredExitsStore();
    DELETE_CTORS_AND_ASSIGN_OPS(DiscoveredExitsStore);

    // Nothing for a missing file. A damaged or foreign file is renamed to "<name>.corrupt" (or
    // the next free ".corrupt.N") so neither a later save nor a later corruption can destroy
    // whatever could still be recovered from it.
    NODISCARD std::optional<DiscoveredExits> load(const QString &character);
    void save(const QString &character, const DiscoveredExits &exits, bool immediate = false);
    void flush();

    NODISCARD bool matches(const QString &directory, const QString &server) const
    {
        return m_directory == directory && m_server == server;
    }
};
