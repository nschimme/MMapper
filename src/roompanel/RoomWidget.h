#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "RoomMob.h"
#include "RoomViewModel.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include <QAbstractTableModel>
#include <QString>
#include <QWidget>
#include <QtCore>

class RoomMobs;
class QObject;
class RoomManager;

class NODISCARD_QOBJECT RoomModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum class NODISCARD ColumnTypeEnum : uint8_t { NAME = 0, LABEL, POSITION, EFFECTS, WEAPON, FIGHTING, MOUNT };
private:
    const RoomMobs &m_room;
    std::unordered_map<RoomMob::Id, SharedRoomMob> m_mobsById;
    std::vector<SharedRoomMob> m_mobVector;
public:
    RoomModel(QObject *parent, const RoomMobs &room);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void update();
};

class NODISCARD_QOBJECT RoomWidget final : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<RoomViewModel> m_viewModel;
    RoomModel m_model;

public:
    explicit RoomWidget(RoomManager &rm, QWidget *parent);
    ~RoomWidget() final;

private slots:
    void slot_update();
};
