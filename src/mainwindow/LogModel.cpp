// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "LogModel.h"

#include <QClipboard>
#include <QGuiApplication>

LogModel::LogModel(QObject *const parent)
    : QAbstractListModel(parent)
{}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_lines.size());
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<qsizetype>(index.row()) >= m_lines.size()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return m_lines.at(index.row());
    }

    return QVariant();
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    return roles;
}

QString LogModel::getText() const
{
    return m_lines.join(QChar::fromLatin1('\n'));
}

void LogModel::append(const QString &mod, const QString &message)
{
    const QString line = QString("[%1] %2").arg(mod, message);

    const int row = static_cast<int>(m_lines.size());
    beginInsertRows(QModelIndex(), row, row);
    m_lines.append(line);
    endInsertRows();

    // If more than MAX_LINES, preserve by deleting from the start
    const int over = static_cast<int>(m_lines.size()) - MAX_LINES;
    if (over > 0) {
        beginRemoveRows(QModelIndex(), 0, over - 1);
        m_lines.erase(m_lines.begin(), m_lines.begin() + over);
        endRemoveRows();
    }

    emit textChanged();
}

void LogModel::clear()
{
    if (!m_lines.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, static_cast<int>(m_lines.size()) - 1);
        m_lines.clear();
        endRemoveRows();
    }
    emit textChanged();
}

void LogModel::copyAll() const
{
    if (auto *const clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(m_lines.join(QChar('\n')));
    }
}
