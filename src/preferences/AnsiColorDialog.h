#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AnsiColorViewModel.h"
#include <QDialog>
#include <memory>
#include <functional>

namespace Ui { class AnsiColorDialog; }

class NODISCARD_QOBJECT AnsiColorDialog final : public QDialog
{
    Q_OBJECT
private:
    AnsiColorViewModel m_viewModel;
    std::unique_ptr<Ui::AnsiColorDialog> m_ui;
public:
    explicit AnsiColorDialog(const QString &ansi, QWidget *parent);
    ~AnsiColorDialog() final;
    static void getColor(const QString &ansi, QWidget *p, std::function<void(QString)> cb);
private:
    void updateUI();
};
