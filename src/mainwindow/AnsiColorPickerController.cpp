// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnsiColorPickerController.h"

#include "../preferences/AnsiColorTables.h"

namespace { // anonymous
NODISCARD const AnsiColorTables::Entry &tableEntry(const int index)
{
    const auto &table = AnsiColorTables::colorTable();
    if (index < 0 || static_cast<size_t>(index) >= table.size()) {
        return table.at(0);
    }
    return table.at(static_cast<size_t>(index));
}
} // namespace

AnsiColorPickerController::AnsiColorPickerController(QObject *const parent)
    : QObject(parent)
{
    regenerate();
}

QStringList AnsiColorPickerController::getColorNames() const
{
    QStringList result;
    for (const AnsiColorTables::Entry &entry : AnsiColorTables::colorTable()) {
        result << entry.description;
    }
    return result;
}

QColor AnsiColorPickerController::swatchColor(const int index) const
{
    const auto &entry = tableEntry(index);
    if (!entry.color.hasColor()) {
        // No single "correct" swatch color for the default/"none" entry
        // (AnsiCombo's widget version picks black or white depending on
        // whether it's the fg or bg combo; this list is shared by both), so
        // use a neutral gray instead.
        return QColor(Qt::gray);
    }
    return mmqt::toQColor(entry.color.getColor());
}

QColor AnsiColorPickerController::getPreviewFg() const
{
    return AnsiColorTables::colorFromString(m_resultAnsiString).getFgColor();
}

QColor AnsiColorPickerController::getPreviewBg() const
{
    return AnsiColorTables::colorFromString(m_resultAnsiString).getBgColor();
}

void AnsiColorPickerController::init(const QString &ansiString)
{
    const AnsiColorTables::ParsedColor color = AnsiColorTables::colorFromString(ansiString);

    const auto &table = AnsiColorTables::colorTable();
    const auto findIndex = [&table](const AnsiColor16 c) -> int {
        for (size_t i = 0; i < table.size(); ++i) {
            if (table[i].color == c) {
                return static_cast<int>(i);
            }
        }
        return 0;
    };

    m_fgIndex = findIndex(color.fg);
    m_bgIndex = findIndex(color.bg);
    m_bold = color.bold;
    m_italic = color.italic;
    m_underline = color.underline;

    regenerate();
}

void AnsiColorPickerController::setFgIndex(const int index)
{
    m_fgIndex = index;
    regenerate();
}

void AnsiColorPickerController::setBgIndex(const int index)
{
    m_bgIndex = index;
    regenerate();
}

void AnsiColorPickerController::setBold(const bool value)
{
    m_bold = value;
    regenerate();
}

void AnsiColorPickerController::setItalic(const bool value)
{
    m_italic = value;
    regenerate();
}

void AnsiColorPickerController::setUnderline(const bool value)
{
    m_underline = value;
    regenerate();
}

void AnsiColorPickerController::regenerate()
{
    m_resultAnsiString = AnsiColorTables::generateAnsiString(tableEntry(m_fgIndex).color,
                                                             tableEntry(m_bgIndex).color,
                                                             m_bold,
                                                             m_italic,
                                                             m_underline);
    emit sig_changed();
}
