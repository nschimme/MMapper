// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GlTerminalBuffer.h"

#include "../configuration/configuration.h"
#include "../global/AnsiOstream.h"

#include <algorithm>
#include <cmath>

#include <QClipboard>
#include <QGuiApplication>

namespace mmgl {

static Color toGlColor(const QColor &qc)
{
    return Color{static_cast<float>(qc.redF()),
                 static_cast<float>(qc.greenF()),
                 static_cast<float>(qc.blueF()),
                 static_cast<float>(qc.alphaF())};
}

void GlTerminalBuffer::setMaxLines(const int maxLines)
{
    m_maxLines = std::max(10, maxLines);
    while (static_cast<int>(m_lines.size()) > m_maxLines) {
        m_lines.pop_front();
    }
}

void GlTerminalBuffer::setColumnsRows(const int cols, const int rows)
{
    if (cols <= 0 || rows <= 0) {
        return;
    }
    m_columns = cols;
    m_rows = rows;
}

void GlTerminalBuffer::clear()
{
    m_lines.clear();
    m_lines.emplace_back();
    m_scrollOffset = 0;
    m_currentStyle = GlTerminalCell{};
    m_currentRawAnsi = RawAnsi{};
    clearSelection();
}

void GlTerminalBuffer::setScrollOffset(const int offset)
{
    const int maxOffset = std::max(0, static_cast<int>(m_lines.size()) - m_rows);
    m_scrollOffset = std::clamp(offset, 0, maxOffset);
}

void GlTerminalBuffer::scroll(const int deltaLines)
{
    setScrollOffset(m_scrollOffset + deltaLines);
}

void GlTerminalBuffer::pushNewLine()
{
    m_lines.emplace_back();
    if (static_cast<int>(m_lines.size()) > m_maxLines) {
        m_lines.pop_front();
        if (m_scrollOffset > 0) {
            m_scrollOffset = std::max(0, m_scrollOffset - 1);
        }
    }
}

void GlTerminalBuffer::pushChar(const uint32_t ch)
{
    if (m_lines.empty()) {
        m_lines.emplace_back();
    }

    auto &currentLine = m_lines.back();
    if (static_cast<int>(currentLine.cells.size()) >= m_columns && m_columns > 0) {
        currentLine.isWrapped = true;
        pushNewLine();
    }

    GlTerminalCell cell = m_currentStyle;
    cell.charCode = ch;
    m_lines.back().cells.push_back(cell);
}

void GlTerminalBuffer::processAnsiSequence(const QStringView ansi)
{
    if (mmqt::isAnsiEraseLine(ansi)) {
        if (!m_lines.empty()) {
            m_lines.back().cells.clear();
        }
        return;
    }

    if (const auto optAnsi = mmqt::parseAnsiColor(m_currentRawAnsi, ansi)) {
        m_currentRawAnsi = *optAnsi;

        const bool isBold = m_currentRawAnsi.getFlags().contains(AnsiStyleFlagEnum::Bold);
        const bool isUnderline = m_currentRawAnsi.getFlags().contains(AnsiStyleFlagEnum::Underline);
        const bool isReverse = m_currentRawAnsi.getFlags().contains(AnsiStyleFlagEnum::Reverse);

        QColor fgQc = mmqt::decodeColor(m_currentRawAnsi.fg,
                                        getConfig().integratedClient.foregroundColor,
                                        isBold);
        m_currentStyle.fgColor = toGlColor(fgQc);
        if (m_currentRawAnsi.bg.hasDefaultColor()) {
            m_currentStyle.bgColor = std::nullopt;
        } else {
            QColor bgQc = mmqt::decodeColor(m_currentRawAnsi.bg,
                                            getConfig().integratedClient.backgroundColor,
                                            false);
            m_currentStyle.bgColor = toGlColor(bgQc);
        }
        m_currentStyle.bold = isBold;
        m_currentStyle.underline = isUnderline;
        m_currentStyle.reversed = isReverse;

        if (m_currentStyle.reversed) {
            Color tempFg = m_currentStyle.fgColor;
            m_currentStyle.fgColor = m_currentStyle.bgColor.value_or(Color{0.0f, 0.0f, 0.0f, 1.0f});
            m_currentStyle.bgColor = tempFg;
        }
    }
}

void GlTerminalBuffer::appendStreamText(const QStringView str)
{
    qsizetype pos = 0;
    const qsizetype len = str.size();

    while (pos < len) {
        const QChar ch = str[pos];

        if (ch == mmqt::QC_ALERT) {
            m_bellTriggered = true;
            ++pos;
            continue;
        }

        if (ch == QChar::fromUcs2('\r')) {
            ++pos;
            continue;
        }

        if (ch == QChar::fromUcs2('\n')) {
            pushNewLine();
            ++pos;
            continue;
        }

        if (ch == QChar::fromUcs2('\b')) {
            if (!m_lines.empty() && !m_lines.back().cells.empty()) {
                m_lines.back().cells.pop_back();
            }
            ++pos;
            continue;
        }

        if (ch == mmqt::QC_ESC && (pos + 1 < len) && str[pos + 1] == QChar::fromUcs2('[')) {
            qsizetype end = pos + 2;
            while (end < len && !str[end].isLetter() && str[end] != QChar::fromUcs2('~')) {
                ++end;
            }
            if (end < len) {
                ++end; // include terminating command character
                const QStringView ansiSeq = str.first(end).last(end - pos);
                processAnsiSequence(ansiSeq);
                pos = end;
                continue;
            }
        }

        pushChar(ch.unicode());
        ++pos;
    }
}

void GlTerminalBuffer::startSelection(const int col, const int lineIndex)
{
    m_hasSelection = true;
    m_selectStart = QPoint{col, lineIndex};
    m_selectEnd = QPoint{col, lineIndex};
}

void GlTerminalBuffer::updateSelection(const int col, const int lineIndex)
{
    if (m_hasSelection) {
        m_selectEnd = QPoint{col, lineIndex};
    }
}

void GlTerminalBuffer::clearSelection()
{
    m_hasSelection = false;
    m_selectStart = QPoint{-1, -1};
    m_selectEnd = QPoint{-1, -1};
}

bool GlTerminalBuffer::isCellSelected(const int col, const int lineIndex) const
{
    if (!m_hasSelection) {
        return false;
    }

    QPoint p1 = m_selectStart;
    QPoint p2 = m_selectEnd;

    if (p1.y() > p2.y() || (p1.y() == p2.y() && p1.x() > p2.x())) {
        std::swap(p1, p2);
    }

    if (lineIndex < p1.y() || lineIndex > p2.y()) {
        return false;
    }

    if (p1.y() == p2.y()) {
        return col >= p1.x() && col <= p2.x();
    }

    if (lineIndex == p1.y()) {
        return col >= p1.x();
    }

    if (lineIndex == p2.y()) {
        return col <= p2.x();
    }

    return true;
}

QString GlTerminalBuffer::getSelectedText() const
{
    if (!m_hasSelection) {
        return {};
    }

    QPoint p1 = m_selectStart;
    QPoint p2 = m_selectEnd;

    if (p1.y() > p2.y() || (p1.y() == p2.y() && p1.x() > p2.x())) {
        std::swap(p1, p2);
    }

    QString result;
    const int startLine = std::clamp(p1.y(), 0, static_cast<int>(m_lines.size()) - 1);
    const int endLine = std::clamp(p2.y(), 0, static_cast<int>(m_lines.size()) - 1);

    for (int lineIdx = startLine; lineIdx <= endLine; ++lineIdx) {
        const auto &line = m_lines.at(static_cast<size_t>(lineIdx));
        const int cellCount = static_cast<int>(line.cells.size());

        int colStart = (lineIdx == p1.y()) ? p1.x() : 0;
        int colEnd = (lineIdx == p2.y()) ? p2.x() : cellCount - 1;

        colStart = std::clamp(colStart, 0, std::max(0, cellCount - 1));
        colEnd = std::clamp(colEnd, 0, std::max(0, cellCount - 1));

        for (int c = colStart; c <= colEnd && c < cellCount; ++c) {
            result.append(QChar(static_cast<char16_t>(line.cells[static_cast<size_t>(c)].charCode)));
        }

        if (lineIdx < endLine && !line.isWrapped) {
            result.append(QChar::fromUcs2('\n'));
        }
    }

    return result;
}

QString GlTerminalBuffer::getAllText() const
{
    QString result;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const auto &line = m_lines[i];
        for (const auto &cell : line.cells) {
            result.append(QChar(static_cast<char16_t>(cell.charCode)));
        }
        if (i + 1 < m_lines.size() && !line.isWrapped) {
            result.append(QChar::fromUcs2('\n'));
        }
    }
    return result;
}

void GlTerminalBuffer::copySelectionToClipboard() const
{
    const QString text = getSelectedText();
    if (!text.isEmpty()) {
        if (auto *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(text);
        }
    }
}

} // namespace mmgl
