#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "FindRoomsViewModel.h"
#include "../mapdata/roomselection.h"
#include <QDialog>
#include <memory>
#include <glm/vec2.hpp>

namespace Ui { class FindRoomsDlg; }
class QTreeWidgetItem;
class QShortcut;
class MapData;

class NODISCARD_QOBJECT FindRoomsDlg final : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::FindRoomsDlg> ui;
    FindRoomsViewModel m_viewModel;
    QTreeWidgetItem *item = nullptr;
    std::unique_ptr<QShortcut> m_showSelectedRoom;

public:
    explicit FindRoomsDlg(MapData &mapData, QWidget *parent = nullptr);
    ~FindRoomsDlg() final;

signals:
    void sig_center(const glm::vec2 &worldPos);
    void sig_newRoomSelection(const SigRoomSelection &);
    void sig_editSelection();
    void sig_log(const QString &, const QString &);

private slots:
    void slot_findClicked();
    void slot_itemDoubleClicked(QTreeWidgetItem *inputItem);
    void slot_showSelectedRoom();
    void updateUI();
};
