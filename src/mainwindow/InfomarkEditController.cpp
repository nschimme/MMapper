// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "InfomarkEditController.h"

#include "../display/InfomarkSelection.h"
#include "../map/ChangeTypes.h"
#include "../map/Changes.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"

#include <utility>

InfomarkEditController::InfomarkEditController(QObject *const parent)
    : QObject(parent)
{}

void InfomarkEditController::setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &is,
                                                  MapData *const md)
{
    // NOTE: the selection is allowed to be null.
    m_selection = is;
    m_mapData = md;

    updateMarkers();
    refresh();
}

void InfomarkEditController::updateMarkers()
{
    m_markers.clear();
    if (m_selection != nullptr) {
        m_markers.reserve(m_selection->size());
    }

    QStringList names;
    names << tr("Create New Marker");

    if (m_selection != nullptr) {
        m_selection->for_each([this, &names](const InfomarkHandle &marker) {
            switch (marker.getType()) {
            case InfomarkTypeEnum::TEXT:
            case InfomarkTypeEnum::LINE:
            case InfomarkTypeEnum::ARROW:
                m_markers.emplace_back(marker.getId());
                names << marker.getText().toQString();
                break;
            }
        });
    }

    m_markerNames = std::move(names);
    emit sig_markerNamesChanged();

    m_currentIndex = (m_selection != nullptr && m_selection->size() == 1) ? 1 : 0;
    emit sig_currentIndexChanged();
}

InfomarkHandle InfomarkEditController::getCurrentInfomark() const
{
    static const InfomarkDb emptyDb;
    if (m_mapData == nullptr) {
        return InfomarkHandle{emptyDb, INVALID_INFOMARK_ID};
    }

    const auto &map = m_mapData->getCurrentMap();
    const auto &db = map.getInfomarkDb();
    if (m_currentIndex <= 0) {
        return InfomarkHandle{db, INVALID_INFOMARK_ID};
    }
    const auto n = static_cast<size_t>(m_currentIndex - 1);
    if (n >= m_markers.size()) {
        return InfomarkHandle{db, INVALID_INFOMARK_ID};
    }
    return db.find(m_markers.at(n));
}

void InfomarkEditController::setCurrentIndex(const int index)
{
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    emit sig_currentIndexChanged();
    refresh();
}

// Ported from InfomarksEditDlg::slot_objectTypeCurrentIndexChanged(), which
// just called updateDialog(). Mirrors a real widget quirk: refresh() below
// snaps type/markerClass back to the current marker's stored values when one
// is selected, so this only "sticks" while currentIndex is 0 (Create New
// Marker).
void InfomarkEditController::setType(const int type)
{
    m_type = type;
    emit sig_typeChanged();
    refresh();
}

// objectClassesList has no currentIndexChanged connection in the widget
// (connectAll() only wires up objectsList and objectType), so this never
// triggers a refresh.
void InfomarkEditController::setMarkerClass(const int cls)
{
    if (m_markerClass == cls) {
        return;
    }
    m_markerClass = cls;
    emit sig_markerClassChanged();
}

// Ported from InfomarksEditDlg::updateDialog().
void InfomarkEditController::refresh()
{
    const InfomarkHandle marker = getCurrentInfomark();
    if (marker) {
        m_type = static_cast<int>(marker.getType());
        emit sig_typeChanged();
        m_markerClass = static_cast<int>(marker.getClass());
        emit sig_markerClassChanged();
    }

    if (!marker) {
        m_text.clear();
        emit sig_textChanged();

        if (m_selection != nullptr) {
            const Coordinate pos1 = m_selection->getPosition1();
            const Coordinate pos2 = m_selection->getPosition2();
            m_x1 = pos1.x;
            m_y1 = pos1.y;
            m_x2 = pos2.x;
            m_y2 = pos2.y;
            m_layer = pos1.z;
        }
        m_angle = 0;

        m_canModify = false;
    } else {
        m_text = marker.getText().toQString();
        emit sig_textChanged();

        const Coordinate pos1 = marker.getPosition1();
        const Coordinate pos2 = marker.getPosition2();
        m_x1 = pos1.x;
        m_y1 = pos1.y;
        m_x2 = pos2.x;
        m_y2 = pos2.y;
        m_layer = pos1.z;
        m_angle = marker.getRotationAngle();

        m_canModify = true;
    }

    emit sig_x1Changed();
    emit sig_y1Changed();
    emit sig_x2Changed();
    emit sig_y2Changed();
    emit sig_layerChanged();
    emit sig_angleChanged();
    emit sig_canModifyChanged();

    emit sig_refreshed();
}

namespace { // anonymous

void fillRawInfomark(RawInfomark &im,
                     const int type,
                     const int cls,
                     const QString &text,
                     const int x1,
                     const int y1,
                     const int x2,
                     const int y2,
                     const int layer,
                     const int angle)
{
    im.setType(static_cast<InfomarkTypeEnum>(type));
    im.setClass(static_cast<InfomarkClassEnum>(cls));
    im.setText(mmqt::makeInfomarkText(text));
    im.setPosition1(Coordinate{x1, y1, layer});
    im.setPosition2(Coordinate{x2, y2, layer});
    im.setRotationAngle(angle);
}

} // namespace

// Ported from InfomarksEditDlg::slot_createClicked() + updateMark(). Unlike
// the widget, text normalization (empty+TEXT -> "New Marker", non-empty+
// non-TEXT -> "") is expected to already have happened in QML before this is
// called (see the header doc comment).
void InfomarkEditController::create(const int type,
                                    const int cls,
                                    const QString &text,
                                    const int x1,
                                    const int y1,
                                    const int x2,
                                    const int y2,
                                    const int layer,
                                    const int angle)
{
    if (m_mapData == nullptr) {
        return;
    }

    RawInfomark im;
    fillRawInfomark(im, type, cls, text, x1, y1, x2, y2, layer, angle);

    if (!m_mapData->applySingleChange(Change{infomark_change_types::AddInfomark{im}})) {
        emit sig_error(tr("Failed to create infomark."));
        return;
    }

    updateMarkers();
    refresh();
}

// Ported from InfomarksEditDlg::slot_modifyClicked() + updateMark().
void InfomarkEditController::modify(const int type,
                                    const int cls,
                                    const QString &text,
                                    const int x1,
                                    const int y1,
                                    const int x2,
                                    const int y2,
                                    const int layer,
                                    const int angle)
{
    if (m_mapData == nullptr) {
        return;
    }

    const InfomarkHandle current = getCurrentInfomark();
    if (!current) {
        return;
    }

    RawInfomark mark = current.getRawCopy();
    fillRawInfomark(mark, type, cls, text, x1, y1, x2, y2, layer, angle);

    if (!m_mapData->applySingleChange(
            Change{infomark_change_types::UpdateInfomark{current.getId(), mark}})) {
        emit sig_error(tr("Failed to modify infomark."));
    }
}
