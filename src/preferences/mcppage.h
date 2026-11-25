#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"

#include <QWidget>

namespace Ui {
class McpPage;
}

class NODISCARD_QOBJECT McpPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::McpPage *const ui;
    Signal2Lifetime m_lifetime;

public:
    explicit McpPage(QWidget *parent);
    ~McpPage() final;

public slots:
    void slot_loadConfig();
};
