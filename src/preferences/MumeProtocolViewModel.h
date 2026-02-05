#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT MumeProtocolViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool useInternalEditor READ useInternalEditor WRITE setUseInternalEditor NOTIFY settingsChanged)
    Q_PROPERTY(QString externalEditorCommand READ externalEditorCommand WRITE setExternalEditorCommand NOTIFY settingsChanged)

public:
    explicit MumeProtocolViewModel(QObject *parent = nullptr);

    NODISCARD bool useInternalEditor() const;
    void setUseInternalEditor(bool v);
    NODISCARD QString externalEditorCommand() const;
    void setExternalEditorCommand(const QString &v);

    void loadConfig();

signals:
    void settingsChanged();
};
