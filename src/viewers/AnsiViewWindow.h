#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "AnsiViewViewModel.h"
#include <QDialog>
#include <memory>
#include <string_view>

class QTextBrowser;

class NODISCARD AnsiViewWindow final : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<QTextBrowser> m_view;
    AnsiViewViewModel m_viewModel;

public:
    explicit AnsiViewWindow(const QString &program, const QString &title, std::string_view message);
    ~AnsiViewWindow() final;

private slots:
    void updateUI();
};

NODISCARD std::unique_ptr<AnsiViewWindow> makeAnsiViewWindow(const QString &program,
                                                             const QString &title,
                                                             std::string_view body);
