// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "FindRoomsModel.h"

#include <utility>

FindRoomsModel::FindRoomsModel(QObject *const parent)
    : QAbstractListModel(parent)
{}

FindRoomsModel::~FindRoomsModel() = default;

int FindRoomsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant FindRoomsModel::data(const QModelIndex &index, const int role) const
{
    if (!isValidRow(index.row())) {
        return QVariant();
    }
    const Entry &entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case ExternalIdRole:
        return QVariant::fromValue(entry.externalId);
    case NameRole:
        return entry.name;
    case ToolTipRole:
        return entry.toolTip;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FindRoomsModel::roleNames() const
{
    return {{ExternalIdRole, "externalId"}, {NameRole, "name"}, {ToolTipRole, "toolTip"}};
}

void FindRoomsModel::setRooms(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

void FindRoomsModel::clear()
{
    if (m_entries.empty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

bool FindRoomsModel::isValidRow(const int row) const
{
    return row >= 0 && static_cast<size_t>(row) < m_entries.size();
}

uint32_t FindRoomsModel::externalIdAt(const int row) const
{
    if (!isValidRow(row)) {
        return 0;
    }
    return m_entries[static_cast<size_t>(row)].externalId;
}

QString FindRoomsModel::toolTipAt(const int row) const
{
    if (!isValidRow(row)) {
        return QString();
    }
    return m_entries[static_cast<size_t>(row)].toolTip;
}
