// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "RoomEditAttrViewModel.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "../map/Change.h"
#include "../map/ChangeTypes.h"

RoomEditAttrViewModel::RoomEditAttrViewModel(MapData &mapData, QObject *p)
    : QObject(p), m_mapData(mapData) {}

void RoomEditAttrViewModel::setSelection(const SharedRoomSelection &sel) {
    m_selection = sel;
    m_roomNames.clear();
    if (m_selection) {
        for (const auto &id : m_selection->getRoomIds()) {
            if (auto room = m_mapData.findRoomHandle(id)) {
                m_roomNames.append(room.getName().toQString());
            }
        }
    }
    emit roomNamesChanged();
    if (!m_roomNames.isEmpty()) setCurrentRoomIndex(0);
    else setCurrentRoomIndex(-1);
}

void RoomEditAttrViewModel::setCurrentRoomIndex(int i) {
    if (m_currentRoomIndex != i) {
        m_currentRoomIndex = i;
        updateFromSelection();
        emit currentRoomIndexChanged();
    }
}

void RoomEditAttrViewModel::updateFromSelection() {
    if (m_currentRoomIndex < 0 || !m_selection || m_currentRoomIndex >= static_cast<int>(m_selection->size())) {
        m_roomDescription = "";
        m_terrain = 0;
        m_ridable = 0;
        m_teleport = 0;
        m_light = 0;
        m_sundeath = 0;
        m_alignment = 0;
        m_roomNote = "";
    } else {
        auto ids = m_selection->getRoomIds();
        auto it = ids.begin();
        std::advance(it, m_currentRoomIndex);
        if (auto room = m_mapData.findRoomHandle(*it)) {
            m_roomDescription = room.getDescription().toQString();
            m_terrain = static_cast<int>(room.getTerrain());
            m_ridable = static_cast<int>(room.getRidable());
            m_teleport = static_cast<int>(room.getTeleport());
            m_light = static_cast<int>(room.getLight());
            m_sundeath = static_cast<int>(room.getSundeath());
            m_alignment = static_cast<int>(room.getAlignment());
            m_roomNote = room.getNote().toQString();
        }
    }
    emit roomDescriptionChanged();
    emit terrainChanged();
    emit ridableChanged();
    emit teleportChanged();
    emit lightChanged();
    emit sundeathChanged();
    emit alignmentChanged();
    emit roomNoteChanged();
}

void RoomEditAttrViewModel::setTerrain(int t) { if (m_terrain != t) { m_terrain = t; emit terrainChanged(); } }
void RoomEditAttrViewModel::setRidable(int r) { if (m_ridable != r) { m_ridable = r; emit ridableChanged(); } }
void RoomEditAttrViewModel::setTeleport(int t) { if (m_teleport != t) { m_teleport = t; emit teleportChanged(); } }
void RoomEditAttrViewModel::setLight(int l) { if (m_light != l) { m_light = l; emit lightChanged(); } }
void RoomEditAttrViewModel::setSundeath(int s) { if (m_sundeath != s) { m_sundeath = s; emit sundeathChanged(); } }
void RoomEditAttrViewModel::setAlignment(int a) { if (m_alignment != a) { m_alignment = a; emit alignmentChanged(); } }
void RoomEditAttrViewModel::setRoomNote(const QString &n) { if (m_roomNote != n) { m_roomNote = n; emit roomNoteChanged(); } }

void RoomEditAttrViewModel::applyNote() {
    if (m_currentRoomIndex < 0 || !m_selection) return;
    auto ids = m_selection->getRoomIds();
    auto it = ids.begin();
    std::advance(it, m_currentRoomIndex);
    RoomId id = *it;

    m_mapData.applySingleChange(Change{room_change_types::ModifyRoomNote{id, TaggedBoxedStringUtf8<struct RoomNoteTag>(m_roomNote.toStdString())}});
}

void RoomEditAttrViewModel::revertNote() { updateFromSelection(); }
void RoomEditAttrViewModel::clearNote() { setRoomNote(""); }
