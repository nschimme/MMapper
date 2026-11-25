// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/configuration.h"
#include "../parser/AbstractParser-Commands.h"
#include "McpBackend.h"
#include "McpToolRegistry.h"
#include "McpTools.h"

#include <QJsonObject>

void mcpRegisterCommandTools(McpToolRegistry &registry,
                             const McpHost &host,
                             McpEventLog &events,
                             McpWaiters &waiters)
{
    // The command table is static, so the tool list is complete and the same whether or not a
    // session exists (tool lists are normally read once, when a client connects).
    for (const SpecialCommandInfo &info : getSpecialCommandInfos()) {
        const QString command = QString::fromLatin1(info.abbrev.getCommand());
        const QString summary = QString::fromStdString(info.summary);

        registry.add(McpTool{
            QStringLiteral("mmapper_") + command,
            QStringLiteral("Run MMapper's `%1` command (typed with the command prefix, usually "
                           "'_'): %2 Needs a session (see open_session). Returns the events it "
                           "caused. Same as send with the line \"<prefix>%1 <args>\"; output a "
                           "command produces later shows up in read_events.")
                .arg(command, summary),
            mcpObjectSchema(
                {{QStringLiteral("args"),
                  mcpStringProperty(QStringLiteral("Everything after the command name; omit for "
                                                   "none. Use `??` as the argument for syntax "
                                                   "help."))}}),
            [&host, &events, &waiters, command](const QJsonObject &args, McpReply reply) {
                const QString arguments = args.value(QStringLiteral("args")).toString().trimmed();
                QString line = QChar::fromLatin1(getConfig().parser.prefixChar) + command;
                if (!arguments.isEmpty()) {
                    line += QLatin1Char(' ') + arguments;
                }
                mcpRunLine(host, events, waiters, line, {}, 0, std::move(reply));
            }});
    }
}
