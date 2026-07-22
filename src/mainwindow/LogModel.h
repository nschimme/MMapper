#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QAbstractListModel>
#include <QStringList>

class NODISCARD_QOBJECT LogModel final : public QAbstractListModel
{
    Q_OBJECT
    // Joined plain-text view of every line, '\n'-separated, backing
    // LogPanel.qml's single read-only selectable text control (mirrors the
    // QTextBrowser this replaces, see mainwindow.cpp's "logWindow").
    Q_PROPERTY(QString text READ getText NOTIFY textChanged)

private:
    QStringList m_lines;

public:
    static constexpr const int MAX_LINES = 10000;

    explicit LogModel(QObject *parent = nullptr);

    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

    NODISCARD QString getText() const;

public slots:
    void append(const QString &mod, const QString &message);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void copyAll() const;

signals:
    void textChanged();
};
