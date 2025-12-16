// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "CharacterBatches.h"
#include "MapCanvas.h"
#include "MapCanvasData.h"
#include "mapcanvas.h"

void MapCanvas::updateCharacterBatches()
{
    if (m_data.isEmpty()) {
        m_batches.characterBatches.reset();
        return;
    }

    if (!m_batches.characterBatches || m_data.getNeedsCharUpdate()) {
        m_batches.characterBatches.emplace(m_mapScreen, m_currentLayer, getTotalScaleFactor());
        CharacterBatches &characterBatches = m_batches.characterBatches.value();

        // IIFE to abuse return to avoid duplicate else branches
        std::invoke([this, &characterBatches]() -> void {
            if (const std::optional<RoomId> opt_pos = m_data.getCurrentRoomId()) {
                const auto &id = opt_pos.value();
                if (const auto room = m_data.findRoomHandle(id)) {
                    const auto &pos = room.getPosition();
                    // draw the characters before the current position
                    characterBatches.incrementCount(pos);
                    drawGroupCharacters(characterBatches);
                    characterBatches.resetCount(pos);

                    // paint char current position
                    const Color color{getConfig().groupManager.color};
                    characterBatches.drawCharacter(pos, color);

                    // paint prespam
                    const auto prespam = m_data.getPath(id, m_prespammedPath.getQueue());
                    characterBatches.drawPreSpammedPath(pos, prespam, color);
                    return;
                } else {
                    // this can happen if the "current room" is deleted
                    // and we failed to clear it elsewhere.
                    m_data.clearSelectedRoom();
                }
            }
            drawGroupCharacters(characterBatches);
        });

        characterBatches.bake(getOpenGL(), m_textures);
        m_data.clearNeedsCharUpdate();
    }
}

void MapCanvas::drawGroupCharacters(CharacterBatches &batches)
{
    if (m_data.isEmpty()) {
        return;
    }

    RoomIdSet drawnRoomIds;
    const Map &map = m_data.getCurrentMap();
    for (const auto &pCharacter : m_groupManager.selectAll()) {
        // Omit player so that they know group members are below them
        if (pCharacter->isYou())
            continue;

        const CGroupChar &character = deref(pCharacter);

        const auto &r = std::invoke([&character, &map]() -> RoomHandle {
            const ServerRoomId srvId = character.getServerId();
            if (srvId != INVALID_SERVER_ROOMID) {
                if (const auto &room = map.findRoomHandle(srvId)) {
                    return room;
                }
            }
            return RoomHandle{};
        });

        // Do not draw the character if they're in an "Unknown" room
        if (!r) {
            continue;
        }

        const RoomId id = r.getId();
        const auto &pos = r.getPosition();
        const auto color = Color{character.getColor()};
        const bool fill = !drawnRoomIds.contains(id);

        batches.drawCharacter(pos, color, fill);
        drawnRoomIds.insert(id);
    }
}
