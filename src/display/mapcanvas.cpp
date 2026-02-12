// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapcanvas.h"

#include "../configuration/configuration.h"
#include "../global/parserutils.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "InfomarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h"
#include "connectionselection.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QGestureEvent>
#include <QMessageLogContext>
#include <QOpenGLDebugMessage>
#include <QSize>
#include <QString>
#include <QToolTip>
#include <QtGui>
#include <QtWidgets>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near // Bad dog, Microsoft; bad dog!!!
#undef far  // Bad dog, Microsoft; bad dog!!!
#endif

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

MapCanvas::MapCanvas(MapData &mapData,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager)
    : QOpenGLWindow{}
    , MapCanvasViewport{static_cast<QWindow &>(*this)}
    , m_mapScreen{*this}
    , m_opengl{}
    , m_glFont{m_opengl}
    , m_data{mapData}
    , m_prespammedPath{prespammedPath}
    , m_groupManager{groupManager}
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) {
        pmc = this;
    }
}

MapCanvas::~MapCanvas()
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == this) {
        pmc = nullptr;
    }

    cleanupOpenGL();
}

MapCanvas *MapCanvas::getPrimary()
{
    return primaryMapCanvas();
}

void MapCanvas::slot_layerUp()
{
    ++m_currentLayer;
    layerChanged();
}

void MapCanvas::slot_layerDown()
{
    --m_currentLayer;
    layerChanged();
}

void MapCanvas::slot_layerReset()
{
    m_currentLayer = 0;
    layerChanged();
}

void MapCanvas::slot_setRoomSelection(const SigRoomSelection &selection)
{
    auto &input = getInputState();
    if (!selection.isValid()) {
        input.m_roomSelection.reset();
    } else {
        input.m_roomSelection = selection.getShared();
        auto &sel = deref(input.m_roomSelection);
        auto &map = m_data;
        sel.removeMissing(map);

        qDebug() << "Updated selection with" << sel.size() << "rooms";
        if (sel.size() == 1) {
            if (const auto r = map.findRoomHandle(sel.getFirstRoomId())) {
                auto message = mmqt::previewRoom(r,
                                                 mmqt::StripAnsiEnum::Yes,
                                                 mmqt::PreviewStyleEnum::ForLog);
                log(message);
            }
        }
    }

    // Let the MainWindow know
    emit sig_newRoomSelection(selection);
    selectionChanged();
}

void MapCanvas::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection)
{
    getInputState().m_connectionSelection = selection;
    emit sig_newConnectionSelection(selection.get());
    selectionChanged();
}

void MapCanvas::slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &selection)
{
    auto &input = getInputState();
    if (input.m_canvasMouseMode == CanvasMouseModeEnum::CREATE_INFOMARKS) {
        input.m_infoMarkSelection = selection;

    } else if (selection == nullptr || selection->empty()) {
        input.m_infoMarkSelection = nullptr;

    } else {
        qDebug() << "Updated selection with" << selection->size() << "infomarks";
        input.m_infoMarkSelection = selection;
    }

    emit sig_newInfomarkSelection(input.m_infoMarkSelection.get());
    selectionChanged();
}

void MapCanvas::slot_onForcedPositionChange()
{
    slot_requestUpdate();
}

void MapCanvas::slot_createRoom()
{
    auto &input = getInputState();
    if (!input.hasSel1()) {
        return;
    }

    const Coordinate c = input.getSel1().getCoordinate();
    if (const auto &existing = m_data.findRoomHandle(c)) {
        return;
    }

    if (m_data.createEmptyRoom(Coordinate{c.x, c.y, m_currentLayer})) {
        // success
    } else {
        // failed!
    }
}

// REVISIT: This function doesn't need to return a shared ptr. Consider refactoring InfomarkSelection?
std::shared_ptr<InfomarkSelection> MapCanvas::getInfomarkSelection(const MouseSel &sel)
{
    static constexpr float CLICK_RADIUS = 10.f;

    const auto center = sel.to_vec3();
    const auto optClickPoint = project(center);
    if (!optClickPoint) {
        // This should never happen, so we'll crash in debug, but let's do something
        // "sane" in release build since the assert will not trigger.
        assert(false);

        // This distance is in world space, but the mouse click is in screen space.
        static_assert(INFOMARK_SCALE % 5 == 0);
        static constexpr auto INFOMARK_CLICK_RADIUS = INFOMARK_SCALE / 5;
        const auto pos = sel.getScaledCoordinate(INFOMARK_SCALE);
        const auto lo = pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0};
        const auto hi = pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0};
        return InfomarkSelection::alloc(m_data, lo, hi);
    }

    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;

    const auto probe = [this, &clickPoint, &maxCoord, &minCoord](const glm::vec2 &offset) -> void {
        const auto coord = unproject_clamped(clickPoint + offset);
        maxCoord = glm::max(maxCoord, coord);
        minCoord = glm::min(minCoord, coord);
    };

    // screen space can rotate relative to world space, and the projected world
    // space positions can be highly ansitropic (e.g. steep vertical angle),
    // so we'll expand the search area by probing in all 45 and 90 degree angles
    // from the mouse. This isn't perfect, but it should be good enough.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            probe(glm::vec2{static_cast<float>(dx) * CLICK_RADIUS,
                            static_cast<float>(dy) * CLICK_RADIUS});
        }
    }

    const auto getScaled = [this](const glm::vec3 &c) -> Coordinate {
        const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE),
                                    m_currentLayer);
        return Coordinate{pos.x, pos.y, pos.z};
    };

    const auto hi = getScaled(maxCoord);
    const auto lo = getScaled(minCoord);

    return InfomarkSelection::alloc(m_data, lo, hi);
}

void MapCanvas::slot_setScroll(const glm::vec2 &worldPos)
{
    m_scroll = worldPos;
    update();
}

void MapCanvas::slot_setHorizontalScroll(const float worldX)
{
    m_scroll.x = worldX;
    update();
}

void MapCanvas::slot_setVerticalScroll(const float worldY)
{
    m_scroll.y = worldY;
    update();
}

void MapCanvas::slot_zoomIn()
{
    m_scaleFactor.logStep(1);
    zoomChanged();
    update();
}

void MapCanvas::slot_zoomOut()
{
    m_scaleFactor.logStep(-1);
    zoomChanged();
    update();
}

void MapCanvas::slot_zoomReset()
{
    m_scaleFactor.set(1.f);
    zoomChanged();
    update();
}

void MapCanvas::onMovement()
{
    const Coordinate &pos = m_data.tryGetPosition().value_or(Coordinate{});
    m_currentLayer = pos.z;
    emit sig_onCenter(pos.to_vec2() + glm::vec2{0.5f, 0.5f});
    update();
}

void MapCanvas::slot_dataLoaded()
{
    onMovement();

    // REVISIT: is the makeCurrent necessary for calling update()?
    // MakeCurrentRaii makeCurrentRaii{*this};
    forceUpdateMeshes();
}

void MapCanvas::slot_moveMarker(const RoomId id)
{
    assert(id != INVALID_ROOMID);
    m_data.setRoom(id);
    onMovement();
}

void MapCanvas::infomarksChanged()
{
    m_batches.infomarksMeshes.reset();
    update();
}

void MapCanvas::layerChanged()
{
    update();
}

void MapCanvas::forceUpdateMeshes()
{
    m_batches.resetExistingMeshesAndIgnorePendingRemesh();
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    update();
}

void MapCanvas::slot_mapChanged()
{
    // REVISIT: Ideally we'd want to only update the layers/chunks
    // that actually changed.
    if ((false)) {
        m_batches.mapBatches.reset();
    }
    update();
}

void MapCanvas::slot_requestUpdate()
{
    update();
}

void MapCanvas::screenChanged()
{
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) {
        return;
    }

    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();

    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));

        // NOTE: The new in-flight mesh will get UI updates when it "finishes".
        // This means we could save the existing raw mesh data and re-finish it
        // without having to compute it again, right?
        m_batches.resetExistingMeshesButKeepPendingRemesh();

        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont();
        font.cleanup();
        font.init();

        update();
    }
}

void MapCanvas::selectionChanged()
{
    update();
    emit sig_selectionChanged();
}

void MapCanvas::graphicsSettingsChanged()
{
    m_opengl.resetNamedColorsBuffer();
    update();
}

void MapCanvas::zoomChanged()
{
    emit sig_zoomChanged(getRawZoom());
}

void MapCanvas::userPressedEscape(bool /*pressed*/)
{
    // TODO: encapsulate the states so we can easily cancel anything that's in use.
    auto &input = getInputState();

    switch (input.m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS:
        break;

    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        slot_clearConnectionSelection(); // calls selectionChanged();
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        input.m_selectedArea = false;
        input.m_roomSelectionMove.reset();
        slot_clearRoomSelection(); // calls selectionChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        // special case for move: right click selects infomarks
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        input.m_infoMarkSelectionMove.reset();
        slot_clearInfomarkSelection(); // calls selectionChanged();
        break;
    }
}

void MapCanvas::log(const QString &msg)
{
    emit sig_log("MapCanvas", msg);
}
