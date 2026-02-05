// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "FindRoomsViewModel.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/GenericFind.h"

FindRoomsViewModel::FindRoomsViewModel(MapData &mapData, QObject *parent)
    : QObject(parent), m_mapData(mapData) {}

void FindRoomsViewModel::setFilterText(const QString &t) {
    if (m_filterText != t) { m_filterText = t; emit filterTextChanged(); }
}

void FindRoomsViewModel::find() {
    // Basic search logic
    if (m_filterText.isEmpty()) return;

    // In a real implementation, this would call m_mapData.findRooms(...)
    // and store the results in a model.
    emit sig_resultsUpdated();
}
