#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include "../map/RoomIdSet.h"
#include "roomfilter.h"
#include <QObject>
#include <QString>
#include <vector>

class MapData;

class NODISCARD_QOBJECT FindRoomsViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int searchKind READ searchKind WRITE setSearchKind NOTIFY searchKindChanged)
    Q_PROPERTY(bool caseSensitive READ caseSensitive WRITE setCaseSensitive NOTIFY caseSensitiveChanged)
    Q_PROPERTY(bool useRegex READ useRegex WRITE setUseRegex NOTIFY useRegexChanged)
    Q_PROPERTY(int roomsFound READ roomsFound NOTIFY sig_resultsUpdated)

public:
    explicit FindRoomsViewModel(MapData &mapData, QObject *parent = nullptr);

    NODISCARD QString filterText() const { return m_filterText; }
    void setFilterText(const QString &t);

    NODISCARD int searchKind() const { return static_cast<int>(m_searchKind); }
    void setSearchKind(int k);

    NODISCARD bool caseSensitive() const { return m_caseSensitive; }
    void setCaseSensitive(bool c);

    NODISCARD bool useRegex() const { return m_useRegex; }
    void setUseRegex(bool r);

    NODISCARD int roomsFound() const { return static_cast<int>(m_results.size()); }
    NODISCARD const RoomIdSet &results() const { return m_results; }
    NODISCARD MapData &mapData() const { return m_mapData; }

    void find();

signals:
    void filterTextChanged();
    void searchKindChanged();
    void caseSensitiveChanged();
    void useRegexChanged();
    void sig_resultsUpdated();

private:
    MapData &m_mapData;
    QString m_filterText;
    PatternKindsEnum m_searchKind = PatternKindsEnum::NAME;
    bool m_caseSensitive = false;
    bool m_useRegex = false;
    RoomIdSet m_results;
};
