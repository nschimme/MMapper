// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#pragma once

#include "../configuration/configuration.h"
#include "../global/Signal2.h"

#include <QDialog>
#include <QPointer>

namespace Ui {
class ConfigDialog;
}

class QListWidgetItem;
class QStackedWidget;

class ConfigDialog final : public QDialog
{
    Q_OBJECT

private:
    struct PageInfo
    {
        QString name;
        QWidget *widget;
        QListWidgetItem *item;
    };

    Ui::ConfigDialog *ui;
    Configuration m_workingConfig = getConfig();
    Configuration m_originalConfig = getConfig();
    Signal2Lifetime m_lifetime;
    QList<PageInfo> m_pages;

public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void slot_changePage(QListWidgetItem *current, QListWidgetItem *previous);
    void slot_apply();
    void slot_ok();
    void slot_cancel();
    void slot_updateApplyButton();
    void slot_search(const QString &text);

signals:
    void sig_loadConfig();

private:
    void createIcons();
    void updateSearch();
};
