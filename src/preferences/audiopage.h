#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"

#include <QWidget>

#ifndef MMAPPER_WITH_MULTIMEDIA
class QLabel;
#endif

namespace Ui {
class AudioPage;
}

class NODISCARD_QOBJECT AudioPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::AudioPage *const ui;

public:
    explicit AudioPage(QWidget *parent);
    ~AudioPage() final;

public slots:
    void slot_loadConfig();
    void slot_musicEnabledChanged(int);
    void slot_musicVolumeChanged(int);
    void slot_soundsEnabledChanged(int);
    void slot_soundsVolumeChanged(int);

signals:
    void sig_audioSettingsChanged();
};
