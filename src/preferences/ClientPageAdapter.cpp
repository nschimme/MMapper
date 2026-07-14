// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ClientPageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "../global/Consts.h"

#include <QColorDialog>
#include <QFont>
#include <QFontDialog>
#include <QFontInfo>

ClientPageAdapter::ClientPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{}

QString ClientPageAdapter::getFontDisplayName() const
{
    QFont font;
    font.fromString(getConfig().integratedClient.font);
    const QFontInfo fi(font);
    return QStringLiteral("%1 %2, %3").arg(fi.family()).arg(fi.styleName()).arg(fi.pointSize());
}

QColor ClientPageAdapter::getForegroundColor() const
{
    return getConfig().integratedClient.foregroundColor;
}

void ClientPageAdapter::setForegroundColor(const QColor &value)
{
    setConfig().integratedClient.foregroundColor = value;
    emit sig_changed();
}

QColor ClientPageAdapter::getBackgroundColor() const
{
    return getConfig().integratedClient.backgroundColor;
}

void ClientPageAdapter::setBackgroundColor(const QColor &value)
{
    setConfig().integratedClient.backgroundColor = value;
    emit sig_changed();
}

int ClientPageAdapter::getColumns() const
{
    return getConfig().integratedClient.columns;
}

void ClientPageAdapter::setColumns(const int value)
{
    setConfig().integratedClient.columns = value;
    emit sig_changed();
}

int ClientPageAdapter::getRows() const
{
    return getConfig().integratedClient.rows;
}

void ClientPageAdapter::setRows(const int value)
{
    setConfig().integratedClient.rows = value;
    emit sig_changed();
}

int ClientPageAdapter::getLinesOfScrollback() const
{
    return getConfig().integratedClient.linesOfScrollback;
}

void ClientPageAdapter::setLinesOfScrollback(const int value)
{
    setConfig().integratedClient.linesOfScrollback = value;
    emit sig_changed();
}

int ClientPageAdapter::getLinesOfPeekPreview() const
{
    return getConfig().integratedClient.linesOfPeekPreview;
}

void ClientPageAdapter::setLinesOfPeekPreview(const int value)
{
    setConfig().integratedClient.linesOfPeekPreview = value;
    emit sig_changed();
}

int ClientPageAdapter::getLinesOfInputHistory() const
{
    return getConfig().integratedClient.linesOfInputHistory;
}

void ClientPageAdapter::setLinesOfInputHistory(const int value)
{
    setConfig().integratedClient.linesOfInputHistory = value;
    emit sig_changed();
}

int ClientPageAdapter::getTabCompletionDictionarySize() const
{
    return getConfig().integratedClient.tabCompletionDictionarySize;
}

void ClientPageAdapter::setTabCompletionDictionarySize(const int value)
{
    setConfig().integratedClient.tabCompletionDictionarySize = value;
    emit sig_changed();
}

bool ClientPageAdapter::getClearInputOnEnter() const
{
    return getConfig().integratedClient.clearInputOnEnter;
}

void ClientPageAdapter::setClearInputOnEnter(const bool value)
{
    setConfig().integratedClient.clearInputOnEnter = value;
    emit sig_changed();
}

bool ClientPageAdapter::getAutoResizeTerminal() const
{
    return getConfig().integratedClient.autoResizeTerminal;
}

void ClientPageAdapter::setAutoResizeTerminal(const bool value)
{
    setConfig().integratedClient.autoResizeTerminal = value;
    emit sig_changed();
}

bool ClientPageAdapter::getAudibleBell() const
{
    return getConfig().integratedClient.audibleBell;
}

void ClientPageAdapter::setAudibleBell(const bool value)
{
    setConfig().integratedClient.audibleBell = value;
    emit sig_changed();
}

bool ClientPageAdapter::getVisualBell() const
{
    return getConfig().integratedClient.visualBell;
}

void ClientPageAdapter::setVisualBell(const bool value)
{
    setConfig().integratedClient.visualBell = value;
    emit sig_changed();
}

bool ClientPageAdapter::getUseCommandSeparator() const
{
    return getConfig().integratedClient.useCommandSeparator;
}

void ClientPageAdapter::setUseCommandSeparator(const bool value)
{
    setConfig().integratedClient.useCommandSeparator = value;
    emit sig_changed();
}

QString ClientPageAdapter::getCommandSeparator() const
{
    return getConfig().integratedClient.commandSeparator;
}

bool ClientPageAdapter::setCommandSeparator(const QString &value)
{
    if (value.length() != 1) {
        return false;
    }
    const QChar c = value.front();
    if (c == char_consts::C_BACKSLASH || !c.isPrint() || c.isSpace()) {
        return false;
    }
    setConfig().integratedClient.commandSeparator = value;
    emit sig_changed();
    return true;
}

void ClientPageAdapter::chooseFont()
{
    auto &fontDescription = setConfig().integratedClient.font;
    QFont oldFont;
    oldFont.fromString(fontDescription);

    bool ok = false;
    const QFont newFont = QFontDialog::getFont(&ok,
                                               oldFont,
                                               m_dialogParent,
                                               tr("Select Font"),
                                               QFontDialog::MonospacedFonts);
    if (ok) {
        fontDescription = newFont.toString();
        emit sig_changed();
    }
}

void ClientPageAdapter::chooseForegroundColor()
{
    const QColor color = QColorDialog::getColor(getForegroundColor(), m_dialogParent);
    if (color.isValid()) {
        setForegroundColor(color);
    }
}

void ClientPageAdapter::chooseBackgroundColor()
{
    const QColor color = QColorDialog::getColor(getBackgroundColor(), m_dialogParent);
    if (color.isValid()) {
        setBackgroundColor(color);
    }
}

void ClientPageAdapter::reload()
{
    emit sig_changed();
}
