#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QWidget>

class AudioManager;
class QPushButton;
class QLabel;

class AudioHint final : public QWidget
{
    Q_OBJECT

public:
    explicit AudioHint(AudioManager &audioManager, QWidget *parent = nullptr);
    ~AudioHint() override;

private:
    AudioManager &m_audioManager;
    QLabel *m_iconLabel;
    QLabel *m_textLabel;
    QPushButton *m_yesButton;
    QPushButton *m_noButton;
};
