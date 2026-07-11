#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "GroupModel.h"

#include <glm/vec2.hpp>

#include <QObject>

class MapData;
class Mmapper2Group;

// Lifts the operations offered by GroupWidget's context menu and drag/drop
// (see groupwidget.cpp) into a QObject that can be driven from either C++ or
// QML. Owns the GroupModel/GroupProxyModel pair and mirrors the
// Mmapper2Group signal wiring GroupWidget's constructor performs, so both
// the QTableView-based GroupWidget (legacy/wasm builds) and this controller
// (QML builds) can independently track the same Mmapper2Group.
class NODISCARD_QOBJECT GroupController final : public QObject
{
    Q_OBJECT

private:
    Mmapper2Group &m_group;
    MapData &m_mapData;
    GroupModel *m_model = nullptr;
    GroupProxyModel *m_proxy = nullptr;

public:
    explicit GroupController(Mmapper2Group &group, MapData &mapData, QObject *parent);

public:
    NODISCARD GroupModel *getModel() { return m_model; }
    NODISCARD GroupProxyModel *getProxy() { return m_proxy; }

public slots:
    void slot_mapLoaded() { m_model->setMapLoaded(true); }
    void slot_mapUnloaded() { m_model->setMapLoaded(false); }

public:
    // proxyRow indices refer to rows in getProxy(), i.e. the row indices QML
    // delegates see.
    Q_INVOKABLE void centerOnCharacter(int proxyRow);
    Q_INVOKABLE void recolorCharacter(int proxyRow);
    Q_INVOKABLE void moveCharacter(int fromProxyRow, int toProxyRow);
    Q_INVOKABLE void refreshFilter();
    NODISCARD Q_INVOKABLE bool canCenter(int proxyRow) const;

signals:
    void sig_center(glm::vec2 pos); // connects to MapWindow

private slots:
    void slot_onCharacterAdded(SharedGroupChar character);
    void slot_onCharacterRemoved(GroupId characterId);
    void slot_onCharacterUpdated(SharedGroupChar character);
    void slot_onGroupReset(const GroupVector &newCharacterList);
};
