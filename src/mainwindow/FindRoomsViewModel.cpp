// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "FindRoomsViewModel.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/GenericFind.h"
#include "roomfilter.h"

FindRoomsViewModel::FindRoomsViewModel(MapData &mapData, QObject *parent)
    : QObject(parent), m_mapData(mapData) {}

void FindRoomsViewModel::setFilterText(const QString &t) {
    if (m_filterText != t) { m_filterText = t; emit filterTextChanged(); }
}

void FindRoomsViewModel::setSearchKind(int k) {
    auto kind = static_cast<PatternKindsEnum>(k);
    if (m_searchKind != kind) { m_searchKind = kind; emit searchKindChanged(); }
}

void FindRoomsViewModel::setCaseSensitive(bool c) {
    if (m_caseSensitive != c) { m_caseSensitive = c; emit caseSensitiveChanged(); }
}

void FindRoomsViewModel::setUseRegex(bool r) {
    if (m_useRegex != r) { m_useRegex = r; emit useRegexChanged(); }
}

void FindRoomsViewModel::find() {
    if (m_filterText.isEmpty()) {
        m_results.clear();
        emit sig_resultsUpdated();
        return;
    }

    RoomFilter filter(m_filterText.toStdString(),
                      m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive,
                      m_useRegex,
                      m_searchKind);

    m_results = m_mapData.genericFind(filter);
    emit sig_resultsUpdated();
}
