#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

// Widget-free QML port of preferences/AnsiColorDialog.{h,cpp,ui} (kept under
// NOT WITH_QML): owns no widgets, exposes AnsiColorDialog's fg/bg AnsiCombo
// selections and bold/italic/underline checkboxes as Q_PROPERTYs, and
// AnsiColorDialog::slot_generateNewAnsiColor()'s color-string rebuild as a
// pair of Q_INVOKABLE setters, mirroring InfomarkEditController's role for
// InfomarksEditDlg (see InfomarkEditController.h). See
// qml/AnsiColorPickerDialog.qml for the QML side.
//
// QML contract (stub-drift guard: keep TestQml.cpp's usage of the real
// controller in sync with this surface):
//   Q_PROPERTY QStringList colorNames -- 17 entries (index 0 "none", then
//     one per AnsiColorTables::colorTable() row), shared by both the fg and
//     bg combos.
//   Q_INVOKABLE QColor swatchColor(int index) -- the color a colorNames[index]
//     row's combo delegate should paint as its swatch.
//   Q_PROPERTY int fgIndex/bgIndex (Q_INVOKABLE setFgIndex()/setBgIndex()) --
//     indices into colorNames.
//   Q_PROPERTY bool bold/italic/underline (Q_INVOKABLE setBold()/setItalic()/
//     setUnderline()).
//   Q_PROPERTY QString resultAnsiString -- the rebuilt ANSI escape-sequence
//     string (e.g. "[1;32m"), recomputed after every setter call above.
//   Q_PROPERTY QColor previewFg/previewBg -- resolved preview colors for the
//     live-preview Label, mirroring AnsiColorDialog::slot_updateColors()'s
//     AnsiCombo::makeWidgetColoured() call.
//   Q_INVOKABLE init(QString ansiString) -- (re)initializes every field from
//     an existing ANSI escape-sequence string, mirroring AnsiColorDialog's
//     constructor.
//   signal sig_changed() -- fired whenever any field (or the derived
//     resultAnsiString/preview colors) changes.
class NODISCARD_QOBJECT AnsiColorPickerController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList colorNames READ getColorNames CONSTANT)
    Q_PROPERTY(int fgIndex READ getFgIndex NOTIFY sig_changed)
    Q_PROPERTY(int bgIndex READ getBgIndex NOTIFY sig_changed)
    Q_PROPERTY(bool bold READ getBold NOTIFY sig_changed)
    Q_PROPERTY(bool italic READ getItalic NOTIFY sig_changed)
    Q_PROPERTY(bool underline READ getUnderline NOTIFY sig_changed)
    Q_PROPERTY(QString resultAnsiString READ getResultAnsiString NOTIFY sig_changed)
    Q_PROPERTY(QColor previewFg READ getPreviewFg NOTIFY sig_changed)
    Q_PROPERTY(QColor previewBg READ getPreviewBg NOTIFY sig_changed)

private:
    int m_fgIndex = 0;
    int m_bgIndex = 0;
    bool m_bold = false;
    bool m_italic = false;
    bool m_underline = false;
    QString m_resultAnsiString;

public:
    explicit AnsiColorPickerController(QObject *parent);

public:
    NODISCARD QStringList getColorNames() const;
    NODISCARD Q_INVOKABLE QColor swatchColor(int index) const;

    NODISCARD int getFgIndex() const { return m_fgIndex; }
    NODISCARD int getBgIndex() const { return m_bgIndex; }
    NODISCARD bool getBold() const { return m_bold; }
    NODISCARD bool getItalic() const { return m_italic; }
    NODISCARD bool getUnderline() const { return m_underline; }
    NODISCARD QString getResultAnsiString() const { return m_resultAnsiString; }
    NODISCARD QColor getPreviewFg() const;
    NODISCARD QColor getPreviewBg() const;

    // Mirrors AnsiColorDialog's constructor: parses ansiString and
    // (re)initializes every field from it.
    Q_INVOKABLE void init(const QString &ansiString);

    Q_INVOKABLE void setFgIndex(int index);
    Q_INVOKABLE void setBgIndex(int index);
    Q_INVOKABLE void setBold(bool value);
    Q_INVOKABLE void setItalic(bool value);
    Q_INVOKABLE void setUnderline(bool value);

private:
    // Mirrors AnsiColorDialog::slot_generateNewAnsiColor(): rebuilds
    // m_resultAnsiString from the current fg/bg/style fields.
    void regenerate();

signals:
    void sig_changed();
};
