#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../configuration/configuration.h"
#include "../global/macros.h"

#include <QDialog>
#include <QString>

class QListWidgetItem;
class QObject;
class QShowEvent;
class QStackedWidget;
class QWidget;

namespace Ui {
class ConfigDialog;
}

class NODISCARD_QOBJECT ConfigDialog final : public QDialog
{
    Q_OBJECT
    friend class TestPreferences;

private:
    Ui::ConfigDialog *const ui;
    QStackedWidget *m_pagesWidget = nullptr;
    Configuration m_workingConfig;
    Configuration m_originalConfig;

public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog() final;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void createIcons();

signals:
    void sig_graphicsSettingsChanged();
    void sig_groupSettingsChanged();
    void sig_loadConfig();

public slots:
    void slot_changePage(QListWidgetItem *current, QListWidgetItem *previous);
    void slot_apply();
    void slot_ok();
    void slot_cancel();
    void slot_updateApplyButton();
};
