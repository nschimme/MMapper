#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../configuration/configuration.h"
#include "../global/macros.h"

#include <QWidget>

namespace Ui {
class AudioPage;
}

class Configuration;

class NODISCARD_QOBJECT AudioPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::AudioPage *const ui;
    Configuration &m_config;

public:
    explicit AudioPage(QWidget *parent, Configuration &config);
    ~AudioPage() final;

public slots:
    void slot_loadConfig();
    void slot_outputDeviceChanged(int);
    void slot_updateDevices();
};
