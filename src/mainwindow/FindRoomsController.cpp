// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "FindRoomsController.h"

#include "../global/TextUtils.h"
#include "../map/Map.h"
#include "../map/coordinate.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomfilter.h"
#include "../mapdata/roomselection.h"
#include "FindRoomsModel.h"

#include <exception>
#include <utility>

#include <QDebug>

FindRoomsController::FindRoomsController(MapData &mapData, QObject *const parent)
    : QObject(parent)
    , m_mapData{mapData}
    , m_model{new FindRoomsModel(this)}
{}

void FindRoomsController::setResultSummary(QString text)
{
    if (m_resultSummary == text) {
        return;
    }
    m_resultSummary = std::move(text);
    emit sig_resultSummaryChanged();
}

// Ported from FindRoomsDlg::slot_findClicked() (findroomsdlg.cpp lines
// 119-172); the try/catch around RoomFilter construction and the "%1 room%2
// found" wording are preserved verbatim, with the QMessageBox::critical()
// replaced by sig_error() (QML shows it inline instead of a native dialog).
void FindRoomsController::find(const QString &text,
                               const int kind,
                               const bool caseSensitive,
                               const bool regex)
{
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const std::string stdText = mmqt::toStdStringUtf8(text);

    std::vector<FindRoomsModel::Entry> entries;

    try {
        RoomFilter filter(stdText, cs, regex, static_cast<PatternKindsEnum>(kind));
        const Map &map = m_mapData.getCurrentMap();
        auto found = m_mapData.genericFind(filter);
        entries.reserve(found.size());
        for (const auto roomId : found) {
            const auto &room = map.getRoomHandle(roomId);
            FindRoomsModel::Entry entry;
            entry.externalId = room.getIdExternal().asUint32();
            entry.name = room.getName().toQString();
            // Tooltip doesn't support ANSI, and there's no way to add formatted text.
            entry.toolTip = mmqt::previewRoom(room,
                                              mmqt::StripAnsiEnum::Yes,
                                              mmqt::PreviewStyleEnum::ForDisplay);
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception &ex) {
        qWarning() << "Exception: " << ex.what();
        emit sig_error(QString::fromUtf8(ex.what()));
    }

    const int count = static_cast<int>(entries.size());
    m_model->setRooms(std::move(entries));
    setResultSummary(tr("%1 room%2 found").arg(count).arg(count == 1 ? "" : "s"));
}

// Ported from the selectButton click handler (findroomsdlg.cpp lines 56-77).
void FindRoomsController::selectRows(const QList<int> &rows)
{
    const auto &map = m_mapData.getCurrentMap();
    RoomIdSet tmpSet;
    glm::vec2 sum{0.f, 0.f};
    for (const int row : rows) {
        const auto id = ExternalRoomId{m_model->externalIdAt(row)};
        if (const auto room = map.findRoomHandle(id)) {
            sum += room.getPosition().to_vec2();
            tmpSet.insert(room.getId());
        }
    }

    if (!tmpSet.empty()) {
        // note: half-room offset to the room center
        const glm::vec2 offset{0.5f, 0.5f};
        const auto worldPos = sum / static_cast<float>(tmpSet.size()) + offset;
        emit sig_center(worldPos);
    }

    auto shared = RoomSelection::createSelection(std::move(tmpSet));
    emit sig_newRoomSelection(SigRoomSelection{std::move(shared)});
}

// Ported from the editButton click handler (findroomsdlg.cpp lines 78-91).
void FindRoomsController::editRows(const QList<int> &rows)
{
    const auto &map = m_mapData.getCurrentMap();
    RoomIdSet set;
    for (const int row : rows) {
        const auto id = ExternalRoomId{m_model->externalIdAt(row)};
        if (const auto room = map.findRoomHandle(id)) {
            set.insert(room.getId());
        }
    }
    auto tmpSel = RoomSelection::createSelection(std::move(set));
    emit sig_newRoomSelection(SigRoomSelection{std::move(tmpSel)});
    emit sig_editSelection();
}

// Ported from FindRoomsDlg::slot_itemDoubleClicked() (findroomsdlg.cpp lines
// 185-203).
void FindRoomsController::activateRow(const int row)
{
    const auto &map = m_mapData.getCurrentMap();
    const auto extId = ExternalRoomId{m_model->externalIdAt(row)};
    if (const auto r = map.findRoomHandle(extId)) {
        const Coordinate c = r.getPosition();
        const auto worldPos = c.to_vec2() + glm::vec2{0.5f, 0.5f};
        emit sig_center(worldPos); // connects to MapWindow
        emit sig_log("FindRooms", m_model->toolTipAt(row));
    }
}

void FindRoomsController::clear()
{
    m_model->clear();
    setResultSummary(QString());
}
