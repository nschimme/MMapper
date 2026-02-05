#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class MapData;

class NODISCARD_QOBJECT FindRoomsViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit FindRoomsViewModel(MapData &mapData, QObject *parent = nullptr);

    NODISCARD QString filterText() const { return m_filterText; }
    void setFilterText(const QString &t);

    void find();

signals:
    void filterTextChanged();
    void sig_resultsUpdated();

private:
    MapData &m_mapData;
    QString m_filterText;
};
