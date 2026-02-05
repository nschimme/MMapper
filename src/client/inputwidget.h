#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "InputViewModel.h"
#include "PaletteManager.h"
#include "../global/macros.h"
#include <QPlainTextEdit>

class NODISCARD_QOBJECT InputWidget final : public QPlainTextEdit
{
    Q_OBJECT
private:
    InputViewModel *m_viewModel;
    PaletteManager m_paletteManager;
    QString m_tabFragment;
    bool m_tabbing = false;
    bool m_handledInShortcutOverride = false;

public:
    explicit InputWidget(InputViewModel *vm, QWidget *parent);
    ~InputWidget() final;

    NODISCARD QSize sizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    void handleInput();
    void tabComplete();
    bool handleCommandInput(Qt::Key key, Qt::KeyboardModifiers mods);
    bool handleTerminalShortcut(int key);
};
