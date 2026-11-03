#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../client/InputHistory.h"
#include "../client/stackedinputwidget.h"
#include "../global/Color.h"

#include <optional>

#include <QKeyEvent>
#include <QString>

namespace mmgl {

class GlInputPrompt final
{
private:
    QString m_text;
    int m_cursorPos = 0;
    InputHistory m_history;
    TabHistory m_tabHistory;
    EchoModeEnum m_echoMode = EchoModeEnum::Visible;
    bool m_focused = true;

public:
    GlInputPrompt() = default;

    void setFocused(bool focused) { m_focused = focused; }
    NODISCARD bool isFocused() const { return m_focused; }

    void setEchoMode(EchoModeEnum mode) { m_echoMode = mode; }
    NODISCARD EchoModeEnum getEchoMode() const { return m_echoMode; }

    void setText(const QString &text);
    NODISCARD QString getText() const { return m_text; }

    NODISCARD int getCursorPos() const { return m_cursorPos; }

    NODISCARD QString getDisplayText() const;

    struct PromptResult final
    {
        bool handled = false;
        std::optional<QString> submitted;
    };

    NODISCARD PromptResult handleKeyPress(QKeyEvent *event);

    void clear();

    void backwardHistory();
    void forwardHistory();
    void tabComplete();

    void cutToClipboard();
    void copyToClipboard() const;
    void pasteFromClipboard();
};

} // namespace mmgl
