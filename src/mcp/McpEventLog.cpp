// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpEventLog.h"

#include <algorithm>
#include <utility>

McpEventLog::McpEventLog(const size_t maxEvents, const size_t maxBytes)
    : m_maxEvents{std::max<size_t>(1, maxEvents)}
    , m_maxBytes{maxBytes}
{}

size_t McpEventLog::costOf(const McpEvent &event)
{
    // Approximate; only used to bound memory.
    return sizeof(McpEvent) + static_cast<size_t>(event.kind.size() + event.text.size()) * 2;
}

uint64_t McpEventLog::append(const QString &kind, const QString &text, const QJsonObject &data)
{
    McpEvent event;
    event.seq = ++m_lastSeq;
    event.kind = kind;
    event.text = text;
    event.data = data;
    m_bytes += costOf(event);
    m_events.push_back(std::move(event));
    trim();
    return m_lastSeq;
}

void McpEventLog::trim()
{
    // Always keep the newest event, even if it alone is over the byte limit.
    while (m_events.size() > 1 && (m_events.size() > m_maxEvents || m_bytes > m_maxBytes)) {
        m_bytes -= costOf(m_events.front());
        m_events.pop_front();
    }
}

McpEventLog::ReadResult McpEventLog::readSince(const uint64_t since,
                                               const std::set<QString> &kinds,
                                               const size_t maxResults) const
{
    ReadResult result;
    result.nextSeq = since;

    if (m_events.empty()) {
        // Nothing retained; if something was ever appended after `since`, it was dropped.
        result.truncated = m_lastSeq > since;
        result.nextSeq = std::max(since, m_lastSeq);
        return result;
    }

    if (since >= m_lastSeq) {
        // Nothing newer than the caller's cursor (this also covers a cursor from the future).
        return result;
    }

    const uint64_t firstSeq = m_events.front().seq;
    result.truncated = since + 1 < firstSeq;

    // Sequence numbers are contiguous, so the first wanted event is found by index.
    const uint64_t startSeq = std::max<uint64_t>(since + 1, firstSeq);
    auto it = m_events.begin() + static_cast<std::ptrdiff_t>(startSeq - firstSeq);

    bool stoppedEarly = false;
    for (; it != m_events.end(); ++it) {
        if (!kinds.empty() && kinds.find(it->kind) == kinds.end()) {
            continue;
        }
        if (maxResults != 0 && result.events.size() >= maxResults) {
            stoppedEarly = true;
            break;
        }
        result.events.push_back(*it);
        result.nextSeq = it->seq;
    }

    if (!stoppedEarly) {
        // Everything up to the end was examined, including events the filter skipped.
        result.nextSeq = m_lastSeq;
    }
    return result;
}
