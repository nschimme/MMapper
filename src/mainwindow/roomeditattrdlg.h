#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "RoomEditAttrViewModel.h"
#include "../mapdata/roomselection.h"
#include <QDialog>
#include <memory>

namespace Ui { class RoomEditAttrDlg; }
class MapData;
class MapCanvas;

class NODISCARD_QOBJECT RoomEditAttrDlg final : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::RoomEditAttrDlg> ui;
    RoomEditAttrViewModel m_viewModel;

public:
    explicit RoomEditAttrDlg(QWidget *parent = nullptr);
    ~RoomEditAttrDlg() final;

    void setRoomSelection(const SharedRoomSelection &, MapData *, MapCanvas *) {}

private slots:
    void updateUI();
};
