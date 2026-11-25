// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "McpBackend.h"

#include "McpTools.h"

McpBackend::McpBackend(const McpHost &host,
                       McpEventLog &events,
                       McpWaiters &waiters,
                       McpToolRegistry &registry)
    : m_session{host.listener}
{
    mcpRegisterStatusTools(registry, host, events, m_session);
    mcpRegisterEventTools(registry, events);
    mcpRegisterSessionTools(registry, host, m_session);
    mcpRegisterDriveTools(registry, host, events, waiters);
}
