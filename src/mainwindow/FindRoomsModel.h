#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <cstdint>
#include <vector>

#include <QAbstractListModel>
#include <QString>
#include <QtCore>

// Backs FindRoomsDialog.qml's results ListView. Rows are populated by
// FindRoomsController::find() (see findroomsdlg.cpp's slot_findClicked() for
// the widget-based precedent this mirrors) and read back by row index for
// selection/edit/activation, the same way FindRoomsDlg re-derives an
// ExternalRoomId from resultTable's column-0 text.
class NODISCARD_QOBJECT FindRoomsModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleEnum {
        ExternalIdRole = Qt::UserRole + 1,
        NameRole,
        ToolTipRole,
    };

    // externalId mirrors ExternalRoomId::asUint32(); stored as a plain
    // uint32_t (rather than including map/roomid.h's ExternalRoomId type
    // here) because roomid.h transitively pulls in global/concepts.h's C++20
    // `concept` declarations, which moc's header parser cannot handle.
    struct NODISCARD Entry final
    {
        uint32_t externalId = 0;
        QString name;
        QString toolTip;
    };

private:
    std::vector<Entry> m_entries;

public:
    explicit FindRoomsModel(QObject *parent);
    ~FindRoomsModel() final;

    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

    void setRooms(std::vector<Entry> entries);
    void clear();

    NODISCARD bool isValidRow(int row) const;
    NODISCARD uint32_t externalIdAt(int row) const;
    NODISCARD QString toolTipAt(int row) const;
};
