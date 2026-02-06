#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ConfigViewModel.h"
#include <QDialog>
#include <memory>

namespace Ui { class ConfigDialog; }
class QListWidgetItem;

class NODISCARD_QOBJECT ConfigDialog final : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<Ui::ConfigDialog> ui;
    ConfigViewModel m_viewModel;

public:
    explicit ConfigDialog(QWidget *parent);
    ~ConfigDialog() final;

signals:
    void sig_graphicsSettingsChanged();
    void sig_groupSettingsChanged();

private slots:
    void slot_changePage(QListWidgetItem *current, QListWidgetItem *previous);
    void updateUI();
};
