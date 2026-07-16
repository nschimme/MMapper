// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "CheckableFlagModel.h"

#include <utility>

CheckableFlagModel::CheckableFlagModel(QObject *const parent)
    : QAbstractListModel(parent)
{}

CheckableFlagModel::~CheckableFlagModel() = default;

int CheckableFlagModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant CheckableFlagModel::data(const QModelIndex &index, const int role) const
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || static_cast<size_t>(row) >= m_rows.size()) {
        return QVariant();
    }
    const Row &r = m_rows[static_cast<size_t>(row)];
    switch (role) {
    case NameRole:
        return r.name;
    case IconSourceRole:
        return r.iconSource;
    case CheckStateRole:
        return static_cast<int>(r.state);
    case CheckableRole:
        return r.checkable;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CheckableFlagModel::roleNames() const
{
    return {{NameRole, "name"},
            {IconSourceRole, "iconSource"},
            {CheckStateRole, "checkState"},
            {CheckableRole, "checkable"}};
}

void CheckableFlagModel::setRows(std::vector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void CheckableFlagModel::setState(const int flagValue, const Qt::CheckState state)
{
    const int row = indexOfFlag(flagValue);
    if (row < 0) {
        return;
    }
    Row &r = m_rows[static_cast<size_t>(row)];
    if (r.state == state) {
        return;
    }
    r.state = state;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {CheckStateRole});
}

void CheckableFlagModel::setCheckable(const int flagValue, const bool checkable)
{
    const int row = indexOfFlag(flagValue);
    if (row < 0) {
        return;
    }
    Row &r = m_rows[static_cast<size_t>(row)];
    if (r.checkable == checkable) {
        return;
    }
    r.checkable = checkable;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {CheckableRole});
}

void CheckableFlagModel::setAllStates(const Qt::CheckState state)
{
    if (m_rows.empty()) {
        return;
    }
    for (Row &r : m_rows) {
        r.state = state;
    }
    emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {CheckStateRole});
}

const CheckableFlagModel::Row *CheckableFlagModel::rowAt(const int row) const
{
    if (row < 0 || static_cast<size_t>(row) >= m_rows.size()) {
        return nullptr;
    }
    return &m_rows[static_cast<size_t>(row)];
}

QUrl CheckableFlagModel::iconUrl(const QString &path)
{
    if (path.isEmpty()) {
        return QUrl();
    }
    if (path.startsWith(QLatin1String(":/"))) {
        return QUrl(QLatin1String("qrc") + path);
    }
    return QUrl::fromLocalFile(path);
}

int CheckableFlagModel::indexOfFlag(const int flagValue) const
{
    for (size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].flagValue == flagValue) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
