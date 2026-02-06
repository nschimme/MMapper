// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "RoomWidget.h"
#include "RoomManager.h"
#include "../global/utils.h"
#include <QVBoxLayout>
#include <QTableView>
#include <QHeaderView>

namespace {
const QVariant empty;
constexpr const uint8_t ROOM_COLUMN_COUNT = 7;
}

RoomModel::RoomModel(QObject *parent, const RoomMobs &room) : QAbstractTableModel(parent), m_room(room) {}
int RoomModel::rowCount(const QModelIndex &) const { return std::max(1, static_cast<int>(m_mobVector.size())); }
int RoomModel::columnCount(const QModelIndex &) const { return ROOM_COLUMN_COUNT; }
QVariant RoomModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<ColumnTypeEnum>(section)) {
            case ColumnTypeEnum::NAME: return "Name";
            case ColumnTypeEnum::LABEL: return "Label";
            case ColumnTypeEnum::POSITION: return "Position";
            case ColumnTypeEnum::EFFECTS: return "Effects";
            case ColumnTypeEnum::WEAPON: return "Weapon";
            case ColumnTypeEnum::FIGHTING: return "Fighting";
            case ColumnTypeEnum::MOUNT: return "Mount";
        }
    }
    return QVariant();
}

SharedRoomMob RoomModel::getMob(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < m_mobVector.size()) return m_mobVector.at(static_cast<size_t>(row));
    return nullptr;
}

RoomMob::Field RoomModel::getField(ColumnTypeEnum column) const {
    switch (column) {
        case ColumnTypeEnum::NAME: return RoomMob::Field::NAME;
        case ColumnTypeEnum::LABEL: return RoomMob::Field::LABELS;
        case ColumnTypeEnum::POSITION: return RoomMob::Field::POSITION;
        case ColumnTypeEnum::EFFECTS: return RoomMob::Field::FLAGS;
        case ColumnTypeEnum::WEAPON: return RoomMob::Field::WEAPON;
        case ColumnTypeEnum::FIGHTING: return RoomMob::Field::FIGHTING;
        case ColumnTypeEnum::MOUNT: return RoomMob::Field::MOUNT;
    }
    std::abort();
}

const QVariant &RoomModel::getMobField(int row, int column) const {
    SharedRoomMob mob = getMob(row);
    if (!mob) return empty;
    const RoomMob::Field i = getField(static_cast<ColumnTypeEnum>(column));
    const QVariant &variant = mob->getField(i);
    if (!variant.canConvert<uint>()) return variant;
    const auto iter = m_mobsById.find(variant.toUInt());
    if (iter != m_mobsById.end()) {
        const SharedRoomMob mob2 = iter->second;
        if (mob2) return mob2->getField(RoomMob::Field::NAME);
    }
    return empty;
}

bool RoomModel::isEnemy(int row, int column) const {
    if (column < static_cast<int>(ColumnTypeEnum::LABEL) || column > static_cast<int>(ColumnTypeEnum::WEAPON)) {
        const auto &variant = getMobField(row, column);
        if (variant == empty || !variant.canConvert<QString>()) return false;
        const QString &str = variant.toString();
        return !str.isEmpty() && str.front() == mmqt::QC_ASTERISK;
    }
    return false;
}

bool RoomModel::isFightingYOU(int row, int column) const {
    if (column == static_cast<int>(ColumnTypeEnum::FIGHTING)) {
        SharedRoomMob mob = getMob(row);
        return mob && mob->getField(RoomMob::Field::FIGHTING).toString() == QStringLiteral("you");
    }
    return false;
}

QVariant RoomModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();
    const int row = index.row();
    const int column = index.column();
    if (role == Qt::DisplayRole) {
        if (isFightingYOU(row, column)) return QStringLiteral("YOU");
        return getMobField(row, column);
    }
    if (role == Qt::BackgroundRole && isEnemy(row, column)) return QColor(Qt::yellow);
    if (role == Qt::ForegroundRole) {
        if (isFightingYOU(row, column)) return QColor(Qt::red);
        if (isEnemy(row, column)) return mmqt::textColor(Qt::yellow);
    }
    return empty;
}

void RoomModel::update() {
    beginResetModel();
    m_room.updateModel(m_mobsById, m_mobVector);
    endResetModel();
}

RoomWidget::RoomWidget(RoomManager &rm, QWidget *parent)
    : QWidget(parent), m_model(this, rm.getRoom())
{
    m_viewModel = std::make_unique<RoomViewModel>(rm, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *table = new QTableView(this);
    table->setSelectionMode(QAbstractItemView::ContiguousSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setModel(&m_model);
    layout->addWidget(table);
    connect(m_viewModel.get(), &RoomViewModel::sig_update, this, &RoomWidget::slot_update);
}
RoomWidget::~RoomWidget() = default;
void RoomWidget::slot_update() { m_model.update(); }
