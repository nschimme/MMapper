// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AppCore.h"

#include "../configuration/configuration.h"
#include "../display/mapcanvas.h"
#include "../global/utils.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "../pathmachine/mmapper2pathmachine.h"
#include "CommandRegistry.h"
#include "UiCommand.h"

NODISCARD static const char *basic_plural(const size_t n)
{
    return (n == 1) ? "" : "s";
}

AppCore::AppCore(MapData &mapData,
                 Mmapper2PathMachine &pathMachine,
                 CommandRegistry &commandRegistry,
                 QObject *const parent)
    : QObject(parent)
    , m_mapData(mapData)
    , m_pathMachine(pathMachine)
    , m_commandRegistry(commandRegistry)
{}

AppCore::~AppCore() = default;

void AppCore::showStatusInternal(const QString &txt, const int durationMs)
{
    emit sig_statusMessage(txt, durationMs);
}

static void setConfigMapMode(const MapModeEnum mode)
{
    setConfig().general.mapMode = mode;
    getConfig().write();
}

void AppCore::setMapperMode(const MapModeEnum mode)
{
    setConfigMapMode(mode);
    emit sig_mapperModeChanged(mode);
}

void AppCore::setCanvasMouseMode(const CanvasMouseModeEnum mode)
{
    if (m_canvas != nullptr) {
        m_canvas->slot_setCanvasMouseMode(mode);
    }
}

void AppCore::layerUp()
{
    if (m_canvas != nullptr) {
        m_canvas->slot_layerUp();
    }
}

void AppCore::layerDown()
{
    if (m_canvas != nullptr) {
        m_canvas->slot_layerDown();
    }
}

void AppCore::layerReset()
{
    if (m_canvas != nullptr) {
        m_canvas->slot_layerReset();
    }
}

void AppCore::onNewRoomSelection(const SigRoomSelection &rs)
{
    const bool isValidSelection = rs.isValid();
    const size_t selSize = !isValidSelection ? 0 : rs.deref().size();

    m_commandRegistry.setGroupEnabled("room.selection", isValidSelection);
    m_commandRegistry.command("room.goto-selected")->setEnabled(selSize == 1);
    m_commandRegistry.command("room.force-update-selected")
        ->setEnabled(selSize == 1 && m_pathMachine.hasLastEvent());

    if (isValidSelection) {
        const auto msg = QString("Selection: %1 room%2").arg(selSize).arg(basic_plural(selSize));
        showStatusLong(msg);
    }
}

void AppCore::onNewConnectionSelection(const bool hasSelection)
{
    m_commandRegistry.setGroupEnabled("connection.selection", hasSelection);
}

bool AppCore::onNewInfomarkSelection(const bool hasSelection, const size_t size)
{
    m_commandRegistry.setGroupEnabled("infomark.selection", hasSelection);

    if (hasSelection) {
        showStatusLong(QString("Selection: %1 mark%2").arg(size).arg((size != 1) ? "s" : ""));
        if (size == 0) {
            // Create a new infomark if its an empty selection
            return true;
        }
    }
    return false;
}

void AppCore::updateRoomOffsetCommands(const SharedRoomSelection &roomSelection)
{
    if (roomSelection != nullptr && roomSelection->size()) {
        auto anyRoomAtOffset = [this, &roomSelection](const Coordinate offset) -> bool {
            const auto &sel = deref(roomSelection);
            for (const RoomId id : sel) {
                const Coordinate here = m_mapData.getRoomHandle(id).getPosition();
                const Coordinate target = here + offset;
                if (m_mapData.findRoomHandle(target)) {
                    return true;
                }
            }
            return false;
        };
        m_commandRegistry.command("room.move-up-selected")
            ->setEnabled(!anyRoomAtOffset(Coordinate(0, 0, 1)));
        m_commandRegistry.command("room.move-down-selected")
            ->setEnabled(!anyRoomAtOffset(Coordinate(0, 0, -1)));
        m_commandRegistry.command("room.merge-up-selected")
            ->setEnabled(anyRoomAtOffset(Coordinate(0, 0, 1)));
        m_commandRegistry.command("room.merge-down-selected")
            ->setEnabled(anyRoomAtOffset(Coordinate(0, 0, -1)));
    }
}
