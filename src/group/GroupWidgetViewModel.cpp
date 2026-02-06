// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "GroupWidgetViewModel.h"
#include "mmapper2group.h"
#include "../configuration/configuration.h"
#include "../mapdata/mapdata.h"
#include <QColorDialog>

GroupWidgetViewModel::GroupWidgetViewModel(Mmapper2Group &group, QObject *parent)
    : QObject(parent), m_group(group)
{
}

void GroupWidgetViewModel::setMapLoaded(bool loaded) {
    if (m_mapLoaded != loaded) {
        m_mapLoaded = loaded;
        emit mapLoadedChanged();
    }
}

void GroupWidgetViewModel::centerOnCharacter(const SharedGroupChar &character, MapData &map) {
    if (!character) return;
    if (character->isYou()) {
        if (const auto &r = map.getCurrentRoom()) {
            emit sig_center(r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f});
            return;
        }
    }
    const ServerRoomId srvId = character->getServerId();
    if (srvId != INVALID_SERVER_ROOMID) {
        if (const auto &r = map.findRoomHandle(srvId)) {
            emit sig_center(r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f});
        }
    }
}

void GroupWidgetViewModel::recolorCharacter(const SharedGroupChar &character, const QColor &newColor) {
    if (!character || !newColor.isValid()) return;
    if (newColor != character->getColor()) {
        character->setColor(newColor);
        if (character->isYou()) {
            setConfig().groupManager.color = newColor;
        }
    }
}
