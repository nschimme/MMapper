// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MumeProtocolPageAdapter.h"

#include "../configuration/configuration.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

MumeProtocolPageAdapter::MumeProtocolPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{}

bool MumeProtocolPageAdapter::getInternalRemoteEditor() const
{
    return getConfig().mumeClientProtocol.internalRemoteEditor;
}

void MumeProtocolPageAdapter::setInternalRemoteEditor(const bool value)
{
    setConfig().mumeClientProtocol.internalRemoteEditor = value;
    emit sig_changed();
}

QString MumeProtocolPageAdapter::getExternalRemoteEditorCommand() const
{
    return getConfig().mumeClientProtocol.externalRemoteEditorCommand;
}

void MumeProtocolPageAdapter::setExternalRemoteEditorCommand(const QString &value)
{
    setConfig().mumeClientProtocol.externalRemoteEditorCommand = value;
    emit sig_changed();
}

void MumeProtocolPageAdapter::browseForEditor()
{
    auto &command = setConfig().mumeClientProtocol.externalRemoteEditorCommand;
    const QFileInfo dirInfo(command);
    const QString dir = dirInfo.exists() ? dirInfo.absoluteDir().absolutePath() : QDir::homePath();
    const QString fileName = QFileDialog::getOpenFileName(m_dialogParent,
                                                          tr("Choose editor..."),
                                                          dir,
                                                          tr("Editor (*)"));
    if (!fileName.isEmpty()) {
        QString escaped = fileName;
        const QString quotedFileName = QString(R"("%1")").arg(escaped.replace(R"(")", R"(\")"));
        command = quotedFileName;
        emit sig_changed();
    }
}

void MumeProtocolPageAdapter::reload()
{
    emit sig_changed();
}
