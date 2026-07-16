#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <vector>

#include <QAbstractListModel>
#include <QString>
#include <QUrl>
#include <QtCore>

// Generic, MapData-free QAbstractListModel backing the checkbox lists in the QML port of
// RoomEditAttrDlg (the mob/load/exit/door flag lists on the Room Attributes dialog's Mob
// Flags, Load Flags, Exit Flags, and Door Flags tabs). Each row is a single flag; callers
// (e.g. a future RoomEditController) populate rows with setRows(), then push incremental
// state changes with setState()/setCheckable()/setAllStates() as the selection or the
// selected room(s) change.
//
// QML contract: bind a ListView's model to this, and read each delegate's roles by name:
//   - name: QString display text for the flag
//   - iconSource: QUrl usable directly as an Image/Icon source (see iconUrl())
//   - checkState: int, one of Qt::Unchecked (0) / Qt::PartiallyChecked (1) / Qt::Checked (2)
//   - checkable: bool; false means the row's checkbox should be disabled (e.g. a flag that's
//     not applicable to the current selection)
class NODISCARD_QOBJECT CheckableFlagModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleEnum {
        NameRole = Qt::UserRole + 1,
        IconSourceRole,
        CheckStateRole,
        CheckableRole,
    };

    struct NODISCARD Row final
    {
        int flagValue = 0;
        QString name;
        QUrl iconSource;
        Qt::CheckState state = Qt::Unchecked;
        bool checkable = true;
    };

private:
    std::vector<Row> m_rows;

public:
    explicit CheckableFlagModel(QObject *parent = nullptr);
    ~CheckableFlagModel() final;

    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

    // Replaces all rows (model reset).
    void setRows(std::vector<Row> rows);

    // Updates a single row's check state by flagValue; no-op if not found. Emits a
    // granular dataChanged() for just that row (not a model reset).
    void setState(int flagValue, Qt::CheckState state);
    // Updates a single row's checkable flag by flagValue; no-op if not found. Emits a
    // granular dataChanged() for just that row (not a model reset).
    void setCheckable(int flagValue, bool checkable);
    // Sets every row's check state to the same value in one dataChanged() spanning all rows.
    void setAllStates(Qt::CheckState state);

    NODISCARD const Row *rowAt(int row) const;

    // Maps a resource/file path to a QUrl usable as a QML Image/Icon source:
    // ":/foo/bar.png" -> QUrl("qrc:/foo/bar.png"); an empty path -> an empty (invalid) QUrl;
    // anything else is treated as a local filesystem path.
    NODISCARD static QUrl iconUrl(const QString &path);

private:
    NODISCARD int indexOfFlag(int flagValue) const;
};
