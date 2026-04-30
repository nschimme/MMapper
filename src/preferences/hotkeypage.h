#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QWidget>

namespace Ui {
class HotkeyPage;
}

class HotkeyModel;

class NODISCARD_QOBJECT HotkeyPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::HotkeyPage *const ui;
    HotkeyModel *m_model;

public:
    explicit HotkeyPage(QWidget *parent = nullptr);
    ~HotkeyPage() final;

public slots:
    void slot_loadConfig();

private slots:
    void slot_onAdd();
    void slot_onRemove();
    void slot_onChangeKey();
};
