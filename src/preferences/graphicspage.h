#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "GraphicsViewModel.h"
#include <QWidget>
#include <memory>

namespace Ui { class GraphicsPage; }
class AdvancedGraphicsGroupBox;

class NODISCARD_QOBJECT GraphicsPage final : public QWidget
{
    Q_OBJECT
private:
    Ui::GraphicsPage *const ui;
    GraphicsViewModel m_viewModel;
    std::unique_ptr<AdvancedGraphicsGroupBox> m_advanced;
public:
    explicit GraphicsPage(QWidget *parent);
    ~GraphicsPage() final;
signals:
    void sig_graphicsSettingsChanged();
public slots:
    void slot_loadConfig();
private:
    void updateUI();
};
