#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "InfoMarksEditViewModel.h"
#include <QDialog>
#include <memory>

namespace Ui { class InfomarksEditDlg; }
class InfomarkSelection;
class MapData;
class MapCanvas;

class NODISCARD_QOBJECT InfomarksEditDlg final : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::InfomarksEditDlg> ui;
    InfoMarksEditViewModel m_viewModel;

public:
    explicit InfomarksEditDlg(QWidget *parent = nullptr);
    ~InfomarksEditDlg() final;

    void setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &, MapData *, MapCanvas *) {}

private slots:
    void updateUI();
};
