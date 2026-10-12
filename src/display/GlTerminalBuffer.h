#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/AnsiTextUtils.h"
#include "../global/Color.h"

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringView>

namespace mmgl {

struct GlTerminalCell final
{
    uint32_t charCode = ' ';
    Color fgColor{0.8f, 0.8f, 0.8f, 1.0f};
    Color bgColor{0.0f, 0.0f, 0.0f, 1.0f};
    bool bold = false;
    bool underline = false;
    bool reversed = false;
};

struct GlTerminalLine final
{
    std::vector<GlTerminalCell> cells;
    bool isWrapped = false;
};

class GlTerminalBuffer final
{
private:
    std::deque<GlTerminalLine> m_lines;
    GlTerminalCell m_currentStyle;
    RawAnsi m_currentRawAnsi{};
    int m_maxLines = 5000;
    int m_columns = 80;
    int m_rows = 24;
    int m_scrollOffset = 0; // 0 = at bottom (newest)

    // Selection
    bool m_hasSelection = false;
    QPoint m_selectStart{-1, -1}; // (col, lineIndex)
    QPoint m_selectEnd{-1, -1};

    // Bell tracking
    bool m_bellTriggered = false;

public:
    GlTerminalBuffer()
    {
        m_lines.emplace_back();
    }

    void setMaxLines(int maxLines);
    NODISCARD int getMaxLines() const { return m_maxLines; }

    void setColumnsRows(int cols, int rows);
    NODISCARD int getColumns() const { return m_columns; }
    NODISCARD int getRows() const { return m_rows; }

    void appendStreamText(QStringView str);
    void appendStreamText(const QString &str) { appendStreamText(QStringView{str}); }

    NODISCARD size_t getLineCount() const { return m_lines.size(); }
    NODISCARD const GlTerminalLine &getLine(size_t index) const { return m_lines.at(index); }

    // Scrolling
    void setScrollOffset(int offset);
    void scroll(int deltaLines);
    NODISCARD int getScrollOffset() const { return m_scrollOffset; }
    NODISCARD bool isAtBottom() const { return m_scrollOffset == 0; }
    void scrollToBottom() { setScrollOffset(0); }

    // Bell
    NODISCARD bool takeBellTriggered()
    {
        bool triggered = m_bellTriggered;
        m_bellTriggered = false;
        return triggered;
    }

    // Text selection
    void startSelection(int col, int lineIndex);
    void updateSelection(int col, int lineIndex);
    void clearSelection();
    NODISCARD bool hasSelection() const { return m_hasSelection; }
    NODISCARD bool isCellSelected(int col, int lineIndex) const;
    NODISCARD QString getSelectedText() const;
    NODISCARD QString getAllText() const;
    void copySelectionToClipboard() const;

    void clear();

private:
    void ensureCursorValid();
    void pushChar(uint32_t ch);
    void pushNewLine();
    void processAnsiSequence(QStringView ansi);
};

} // namespace mmgl
