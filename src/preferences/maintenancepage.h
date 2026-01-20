#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/macros.h"
#include <QWidget>

namespace Ui {
class MaintenancePage;
}

class MainWindow;

class NODISCARD_QOBJECT MaintenancePage final : public QWidget
{
    Q_OBJECT

public:
    explicit MaintenancePage(MainWindow *mainWindow, QWidget *parent = nullptr);
    ~MaintenancePage() final;

public slots:
    void slot_loadConfig();

signals:
    void sig_factoryReset();
    void sig_loadConfig();

private slots:
    void slot_exportConfiguration();
    void slot_importConfiguration();
    void slot_factoryReset();

private:
    Ui::MaintenancePage *ui;
    MainWindow *m_mainWindow;
};
