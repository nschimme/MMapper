#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "../global/ConfigEnums.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT MainViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY windowTitleChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY isModifiedChanged)
    Q_PROPERTY(MapModeEnum mapMode READ mapMode WRITE setMapMode NOTIFY mapModeChanged)

public:
    explicit MainViewModel(QObject *parent = nullptr);

    NODISCARD QString windowTitle() const;
    NODISCARD bool isModified() const { return m_isModified; }
    void setModified(bool m);
    NODISCARD MapModeEnum mapMode() const { return m_mapMode; }
    void setMapMode(MapModeEnum m);

    void newFile();
    void openFile(const QString &path);
    void saveFile();
    void saveFileAs(const QString &path);

signals:
    void windowTitleChanged();
    void isModifiedChanged();
    void mapModeChanged();
    void sig_error(const QString &msg);
    void sig_statusMessage(const QString &msg);

private:
    QString m_currentFilePath;
    bool m_isModified = false;
    MapModeEnum m_mapMode = MapModeEnum::PLAY;
};
