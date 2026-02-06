#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "PreviewViewModel.h"
#include "displaywidget.h"
#include <QTextEdit>

class NODISCARD_QOBJECT PreviewWidget final : public QTextEdit
{
    Q_OBJECT
private:
    PreviewViewModel m_viewModel;
    AnsiTextHelper helper;

public:
    explicit PreviewWidget(QWidget *parent = nullptr);
    ~PreviewWidget() final = default;

    void init(const QFont &mainDisplayFont);
    void displayText(const QString &fullText) { m_viewModel.setText(fullText); }

private slots:
    void updateUI();
};
