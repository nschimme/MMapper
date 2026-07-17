#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Jan 'Kovis' Struhar <kovis@sourceforge.net> (Kovis)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/AnsiTextUtils.h"
#include "../global/macros.h"
#include "AnsiColorTables.h"

#include <vector>

#include <QColor>
#include <QComboBox>
#include <QIcon>
#include <QString>
#include <QVector>
#include <QtCore>

class QObject;
class QWidget;

class NODISCARD_QOBJECT AnsiCombo : public QComboBox
{
    Q_OBJECT

private:
    using super = QComboBox;

public:
    struct NODISCARD AnsiItem final
    {
        QString description;
        QIcon picture;

        size_t ui_index = 0;
        AnsiColor16 color;
        AnsiColor16LocationEnum loc = AnsiColor16LocationEnum::Foreground;
    };

private:
    struct NODISCARD FlatMap final
    {
    private:
        std::vector<AnsiItem> values;

    private:
        NODISCARD size_t getInternalIndex(const AnsiColor16 color) const
        {
            const auto &opt = color.color;
            if (opt.has_value()) {
                return static_cast<size_t>(opt.value());
            }
            return 16; // default
        }

    public:
        NODISCARD const AnsiItem &getItemAtUiIndex(const size_t idx) const
        {
            const auto &item = values.at(idx);
            assert(item.ui_index == idx);
            return item;
        }

        NODISCARD const AnsiItem &getItem(AnsiColor16 color) const
        {
            for (auto &item : values) {
                if (item.color == color) {
                    return item;
                }
            }

            assert(false);
            std::abort();
        }

        void insert(AnsiItem item)
        {
            assert(values.size() < 17);
            item.ui_index = values.size();
            values.emplace_back(item);
        }
        void clear() { values.clear(); }
    };

private:
    // There's not really a good default value for this.
    AnsiColor16LocationEnum m_mode = AnsiColor16LocationEnum::Foreground;
    FlatMap m_map;

public:
    static void makeWidgetColoured(QWidget *, const QString &ansiColor, bool changeText = true);

    explicit AnsiCombo(AnsiColor16LocationEnum mode, QWidget *parent);
    explicit AnsiCombo(QWidget *parent);

    void initColours(AnsiColor16LocationEnum mode);

    NODISCARD AnsiColor16LocationEnum getMode() const { return m_mode; }

    NODISCARD AnsiColor16 getAnsiCode() const;
    void setAnsiCode(AnsiColor16);

    // Kept as an alias (rather than a fresh copy of the same fields) so
    // existing callers of AnsiCombo::AnsiColor's members keep working
    // unchanged; the real definition and colorFromString() logic now live in
    // the widget-free AnsiColorTables.h (shared with
    // preferences/AnsiColorPickerController.h, the QML port of
    // AnsiColorDialog).
    using AnsiColor = AnsiColorTables::ParsedColor;

    /// \return true if string is valid ANSI color code
    NODISCARD static AnsiColor colorFromString(const QString &ansiString);
};
