#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias 'Mew_' Viklund <devmew@exedump.com> (Mirnir)

#include "../configuration/configuration.h"
#include "../global/macros.h"

#include <QWidget>
#include <QtCore>

namespace Ui {
class AutoLogPage;
}

class Configuration;

class NODISCARD_QOBJECT AutoLogPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::AutoLogPage *const ui;
    Configuration &m_config;

public:
    explicit AutoLogPage(QWidget *parent, Configuration &config);
    ~AutoLogPage() final;

public slots:
    void slot_loadConfig();
    void slot_logStrategyChanged(int);
    void slot_selectLogLocationButtonClicked(int);
};
