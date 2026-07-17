#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <string_view>

#include <QObject>
#include <QString>

// Widget-free content holder for the ANSI viewer dialog (see
// qml/AnsiViewDialog.qml and the widget-era AnsiViewWindow.{h,cpp}, kept
// under NOT WITH_QML). Renders program/title/message (the same three
// arguments AnsiViewWindow's constructor and makeAnsiViewWindow() take) into
// a title string and an HTML document via mmqt::ansiToHtml(), using the same
// default colors AnsiTextHelper::init() (see client/displaywidget.cpp)
// resolves from Configuration::integratedClient.
//
// Both properties are CONSTANT: this object is built once per dialog
// instance and never mutated afterwards, mirroring AnsiViewWindow's
// read-only QTextBrowser.
class NODISCARD_QOBJECT AnsiViewContent final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString title READ getTitle CONSTANT)
    Q_PROPERTY(QString html READ getHtml CONSTANT)

private:
    QString m_title;
    QString m_html;

public:
    explicit AnsiViewContent(const QString &program,
                             const QString &title,
                             std::string_view message,
                             QObject *parent);

public:
    NODISCARD const QString &getTitle() const { return m_title; }
    NODISCARD const QString &getHtml() const { return m_html; }
};
