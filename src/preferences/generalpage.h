#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../configuration/PasswordConfig.h"
#include "../global/macros.h"

#include <QString>
#include <QWidget>
#include <QtCore>

namespace Ui {
class GeneralPage;
}

class Configuration;

class NODISCARD_QOBJECT GeneralPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::GeneralPage *const ui;
    Configuration &m_config;
    PasswordConfig passCfg;

public:
    explicit GeneralPage(QWidget *parent, Configuration &config);
    ~GeneralPage() final;

signals:
    void sig_reloadConfig();

public slots:
    void slot_loadConfig();
    void slot_remoteNameTextChanged(const QString &);
    void slot_remotePortValueChanged(int);
    void slot_localPortValueChanged(int);

    void slot_emulatedExitsStateChanged(bool);
    void slot_showHiddenExitFlagsStateChanged(bool);
    void slot_showNotesStateChanged(bool);

    void slot_autoLoadFileNameTextChanged(const QString &);
    void slot_autoLoadCheckStateChanged(bool);
    void slot_selectWorldFileButtonClicked(bool);

    void slot_displayMumeClockStateChanged(bool);
    void slot_displayXPStatusStateChanged(bool);
    void slot_themeComboBoxChanged(int);
};
