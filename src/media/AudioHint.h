#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QWidget>

class AudioManager;
class QPushButton;
class QLabel;

class NODISCARD_QOBJECT AudioHint final : public QWidget
{
    Q_OBJECT

public:
    explicit AudioHint(AudioManager &audioManager, QWidget *parent);
    ~AudioHint() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    AudioManager &m_audioManager;
    QWidget *m_container;
    QLabel *m_iconLabel;
    QLabel *m_textLabel;
    QPushButton *m_button;

    void updatePosition();
};
