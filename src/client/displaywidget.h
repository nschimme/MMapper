#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "DisplayViewModel.h"
#include "../global/AnsiTextUtils.h"
#include "../global/macros.h"

#include <optional>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTimer>

struct NODISCARD FontDefaults final
{
    QFont serverOutputFont;
    QColor defaultBg = Qt::black;
    QColor defaultFg = Qt::lightGray;
    std::optional<QColor> defaultUl;

    explicit FontDefaults();
    NODISCARD QColor getDefaultUl() const { return defaultUl.value_or(defaultFg); }
};

extern void setDefaultFormat(QTextCharFormat &format, const FontDefaults &defaults);
NODISCARD extern RawAnsi updateFormat(QTextCharFormat &format, const FontDefaults &defaults, const RawAnsi &before, RawAnsi updated);

struct NODISCARD AnsiTextHelper final
{
    QTextEdit &textEdit;
    QTextCursor cursor;
    QTextCharFormat format;
    const FontDefaults defaults;
    RawAnsi currentAnsi;

    explicit AnsiTextHelper(QTextEdit &input_textEdit, FontDefaults def);
    explicit AnsiTextHelper(QTextEdit &input_textEdit);

    void init();
    void displayText(const QStringView str);
    void limitScrollback(int lineLimit);
};

extern void setAnsiText(QTextEdit *pEdit, std::string_view text);
class NODISCARD_QOBJECT DisplayWidget final : public QTextBrowser
{
    Q_OBJECT
private:
    DisplayViewModel m_viewModel;
    AnsiTextHelper m_ansiTextHelper;
    QTimer *m_visualBellTimer = nullptr;
    bool m_canCopy = false;

public:
    explicit DisplayWidget(QWidget *parent = nullptr);
    ~DisplayWidget() final;

    DisplayViewModel* viewModel() { return &m_viewModel; }

public:
    NODISCARD bool canCopy() const { return m_canCopy; }
    NODISCARD QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void sig_windowSizeChanged(int cols, int rows);
    void sig_returnFocusToInput();
    void sig_showPreview(bool visible);

public slots:
    void slot_displayText(const QStringView str);
private:
    void handleVisualBell();
};
