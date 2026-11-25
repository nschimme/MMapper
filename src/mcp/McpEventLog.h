#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <vector>

#include <QJsonObject>
#include <QString>

// One thing that happened (MUD text, a GMCP message, a position change, ...).
// Sequence numbers start at 1 and only ever increase, so a client can remember the last
// one it saw and ask for everything after it.
struct NODISCARD McpEvent final
{
    uint64_t seq = 0;
    QString kind;
    QString text;
    QJsonObject data;
};

// Bounded, append-only log of events. When it is full the oldest events are dropped; readers
// are told when that happened so a test can tell "nothing happened" from "we lost events".
class NODISCARD McpEventLog final
{
public:
    struct NODISCARD ReadResult final
    {
        std::vector<McpEvent> events;
        // Pass this as `since` on the next read to continue where this one stopped.
        uint64_t nextSeq = 0;
        // True if events newer than `since` were dropped before they could be read.
        bool truncated = false;
    };

private:
    std::deque<McpEvent> m_events;
    size_t m_maxEvents;
    size_t m_maxBytes;
    size_t m_bytes = 0;
    uint64_t m_lastSeq = 0;

public:
    explicit McpEventLog(size_t maxEvents = 10'000, size_t maxBytes = 8 * 1024 * 1024);

    // Returns the sequence number assigned to the event.
    ALLOW_DISCARD uint64_t append(const QString &kind,
                                  const QString &text,
                                  const QJsonObject &data = {});

    NODISCARD uint64_t lastSeq() const { return m_lastSeq; }
    NODISCARD size_t size() const { return m_events.size(); }

    // Events with seq > since, oldest first. An empty `kinds` set means every kind.
    // At most `maxResults` events are returned (0 means no limit).
    NODISCARD ReadResult readSince(uint64_t since,
                                   const std::set<QString> &kinds = {},
                                   size_t maxResults = 0) const;

private:
    NODISCARD static size_t costOf(const McpEvent &event);
    void trim();
};
