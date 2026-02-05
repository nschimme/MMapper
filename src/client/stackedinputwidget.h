#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "StackedInputViewModel.h"
#include <QStackedWidget>

class InputWidget;
class PasswordDialog;

class NODISCARD_QOBJECT StackedInputWidget final : public QStackedWidget
{
    Q_OBJECT
private:
    StackedInputViewModel m_viewModel;
    InputWidget *m_inputWidget;
    PasswordDialog *m_passwordDialog;

public:
    explicit StackedInputWidget(QWidget *parent = nullptr);
    ~StackedInputWidget() final;

    StackedInputViewModel* viewModel() { return &m_viewModel; }

public slots:
    void slot_cut();
    void slot_copy();
    void slot_paste();
};
