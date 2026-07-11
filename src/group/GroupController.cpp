// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GroupController.h"

#include "../configuration/configuration.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "CGroupChar.h"
#include "mmapper2group.h"

#include <tuple>

#include <QColorDialog>

GroupController::GroupController(Mmapper2Group &group, MapData &mapData, QObject *const parent)
    : QObject(parent)
    , m_group(group)
    , m_mapData(mapData)
{
    m_model = new GroupModel(this);
    m_model->setCharacters(m_group.selectAll());

    m_proxy = new GroupProxyModel(this);
    m_proxy->setSourceModel(m_model);

    // Mirrors the Mmapper2Group signal wiring GroupWidget's constructor
    // performs (see groupwidget.cpp); each build (QML or legacy widget)
    // tracks the same Mmapper2Group independently.
    connect(&m_group,
            &Mmapper2Group::sig_characterAdded,
            this,
            &GroupController::slot_onCharacterAdded);
    connect(&m_group,
            &Mmapper2Group::sig_characterRemoved,
            this,
            &GroupController::slot_onCharacterRemoved);
    connect(&m_group,
            &Mmapper2Group::sig_characterUpdated,
            this,
            &GroupController::slot_onCharacterUpdated);
    connect(&m_group, &Mmapper2Group::sig_groupReset, this, &GroupController::slot_onGroupReset);
}

void GroupController::slot_onCharacterAdded(SharedGroupChar character)
{
    assert(character);
    m_model->insertCharacter(character);
}

void GroupController::slot_onCharacterRemoved(const GroupId characterId)
{
    assert(characterId != INVALID_GROUPID);
    m_model->removeCharacterById(characterId);
}

void GroupController::slot_onCharacterUpdated(SharedGroupChar character)
{
    assert(character);
    m_model->updateCharacter(character);
}

void GroupController::slot_onGroupReset(const GroupVector &newCharacterList)
{
    m_model->setCharacters(newCharacterList);
}

bool GroupController::canCenter(const int proxyRow) const
{
    const QModelIndex proxyIndex = m_proxy->index(proxyRow, 0);
    if (!proxyIndex.isValid()) {
        return false;
    }
    // Reuses GroupModel::CanCenterRole so this stays in lockstep with the
    // canCenter QML role exposed on individual rows. GroupModel::data() is
    // protected (only Qt's view internals call it directly), but
    // QSortFilterProxyModel::data() is public and forwards through
    // mapToSource() on our behalf.
    return m_proxy->data(proxyIndex, GroupModel::CanCenterRole).toBool();
}

void GroupController::centerOnCharacter(const int proxyRow)
{
    const QModelIndex proxyIndex = m_proxy->index(proxyRow, 0);
    if (!proxyIndex.isValid()) {
        return;
    }
    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    if (!sourceIndex.isValid()) {
        return;
    }
    const SharedGroupChar character = m_model->getCharacter(sourceIndex.row());
    if (!character) {
        return;
    }

    // Mirrors GroupWidget's m_center QAction lambda in groupwidget.cpp.
    if (character->isYou()) {
        if (const auto &r = m_mapData.getCurrentRoom()) {
            const auto vec2 = r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f};
            emit sig_center(vec2);
            return;
        }
    }

    const ServerRoomId srvId = character->getServerId();
    if (srvId != INVALID_SERVER_ROOMID) {
        if (const auto &r = m_mapData.findRoomHandle(srvId)) {
            const auto vec2 = r.getPosition().to_vec2() + glm::vec2{0.5f, 0.5f};
            emit sig_center(vec2);
        }
    }
}

void GroupController::recolorCharacter(const int proxyRow)
{
    const QModelIndex proxyIndex = m_proxy->index(proxyRow, 0);
    if (!proxyIndex.isValid()) {
        return;
    }
    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    if (!sourceIndex.isValid()) {
        return;
    }
    const SharedGroupChar character = m_model->getCharacter(sourceIndex.row());
    if (!character) {
        return;
    }

    // Mirrors GroupWidget's m_recolor QAction lambda in groupwidget.cpp.
    const QColor newColor
        = QColorDialog::getColor(character->getColor(),
                                 nullptr,
                                 tr("Recolor %1").arg(character->getName().toQString()));
    if (newColor.isValid() && newColor != character->getColor()) {
        character->setColor(newColor);
        if (character->isYou()) {
            setConfig().groupManager.color = newColor;
        }
        // Unlike GroupWidget (whose QTableView delegate re-reads the
        // character's color fresh on every paint), the QML ListView needs an
        // explicit dataChanged to notice the in-place mutation.
        m_model->updateCharacter(character);
    }
}

void GroupController::moveCharacter(const int fromProxyRow, const int toProxyRow)
{
    const QModelIndex fromProxyIndex = m_proxy->index(fromProxyRow, 0);
    const QModelIndex toProxyIndex = m_proxy->index(toProxyRow, 0);
    if (!fromProxyIndex.isValid() || !toProxyIndex.isValid()) {
        return;
    }
    const QModelIndex fromSourceIndex = m_proxy->mapToSource(fromProxyIndex);
    const QModelIndex toSourceIndex = m_proxy->mapToSource(toProxyIndex);
    if (!fromSourceIndex.isValid() || !toSourceIndex.isValid()) {
        return;
    }
    std::ignore = m_model->moveRow(fromSourceIndex.row(), toSourceIndex.row());
}

void GroupController::refreshFilter()
{
    m_proxy->invalidate();
}
