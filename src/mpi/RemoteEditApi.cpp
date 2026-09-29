// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "RemoteEditApi.h"

#include "remoteedit.h"

void RemoteEditApi::reportStatus(AnsiOstream &aos) const
{
    m_remoteEdit.reportStatus(aos);
}

bool RemoteEditApi::reportStatus(AnsiOstream &aos, const uint32_t id) const
{
    return m_remoteEdit.reportStatus(aos, RemoteInternalId{id});
}

bool RemoteEditApi::cancel(const uint32_t id)
{
    const auto &sessions = m_remoteEdit.getSessions();
    const auto it = sessions.find(RemoteInternalId{id});
    if (it == sessions.end()) {
        return false;
    }
    m_remoteEdit.cancelEdit(it->second.get());
    return true;
}

bool RemoteEditApi::discard(const uint32_t id)
{
    const auto &sessions = m_remoteEdit.getSessions();
    const auto it = sessions.find(RemoteInternalId{id});
    if (it == sessions.end()) {
        return false;
    }
    if (it->second->isDraftView()) {
        m_remoteEdit.discardDraft(it->second.get());
    } else {
        m_remoteEdit.cancelEdit(it->second.get());
    }
    return true;
}

void RemoteEditApi::simulateEdit(const QString &title)
{
    static int32_t nextFakeId = 1000000;
    m_remoteEdit.slot_remoteEdit(RemoteSessionId{nextFakeId++},
                                 title,
                                 QString(
                                     "Simulated edit \"%1\".\nType here; nothing is sent to MUME.\n")
                                     .arg(title));
}
