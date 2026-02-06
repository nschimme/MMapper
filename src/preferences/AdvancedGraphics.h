#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AdvancedGraphicsViewModel.h"
#include <QGroupBox>
#include <QCheckBox>
#include <memory>
#include <vector>

class SliderSpinboxButton;
class NODISCARD_QOBJECT AdvancedGraphicsGroupBox final : public QObject
{
    Q_OBJECT
private:
    QGroupBox *const m_groupBox;
    AdvancedGraphicsViewModel m_viewModel;
    std::vector<std::unique_ptr<SliderSpinboxButton>> m_ssbs;
    QCheckBox *m_checkboxDiag;
    QCheckBox *m_checkbox3d;
    QCheckBox *m_autoTilt;

public:
    explicit AdvancedGraphicsGroupBox(QGroupBox &groupBox);
    ~AdvancedGraphicsGroupBox() final;

signals:
    void sig_graphicsSettingsChanged();

private slots:
    void updateUI();
};
