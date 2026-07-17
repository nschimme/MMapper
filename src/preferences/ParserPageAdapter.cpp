// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ParserPageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/Charset.h"
#include "AnsiColorTables.h"

#include <QDebug>

#ifndef MMAPPER_WITH_QML
#include "AnsiColorDialog.h"
#endif

ParserPageAdapter::ParserPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
#ifndef MMAPPER_WITH_QML
    // See the ColorPicker doc comment in ParserPageAdapter.h: under
    // MMAPPER_WITH_QML, MainWindow overrides this via setColorPicker() once
    // it constructs the QmlDialog-hosting equivalent; this .cpp can't do
    // that itself (see the class doc comment for why).
    m_colorPicker = [](const QString &ansiString,
                       QWidget *const parent,
                       std::function<void(QString)> callback) {
        AnsiColorDialog::getColor(ansiString, parent, std::move(callback));
    };
#endif
}

QString ParserPageAdapter::getRoomNameColor() const
{
    return getConfig().parser.roomNameColor;
}

QString ParserPageAdapter::getRoomDescColor() const
{
    return getConfig().parser.roomDescColor;
}

QColor ParserPageAdapter::getRoomNameColorFg() const
{
    return AnsiColorTables::colorFromString(getRoomNameColor()).getFgColor();
}

QColor ParserPageAdapter::getRoomNameColorBg() const
{
    return AnsiColorTables::colorFromString(getRoomNameColor()).getBgColor();
}

QColor ParserPageAdapter::getRoomDescColorFg() const
{
    return AnsiColorTables::colorFromString(getRoomDescColor()).getFgColor();
}

QColor ParserPageAdapter::getRoomDescColorBg() const
{
    return AnsiColorTables::colorFromString(getRoomDescColor()).getBgColor();
}

QString ParserPageAdapter::getPrefixChar() const
{
    return QString(QChar::fromLatin1(getConfig().parser.prefixChar));
}

bool ParserPageAdapter::setPrefixChar(const QString &value)
{
    if (value.length() != 1) {
        return false;
    }
    const char c = mmqt::toLatin1(value[0]);
    // Mirrors isValidPrefix() (see ../parser/AbstractParser-Utils.cpp), which
    // just forwards to charset::ascii::isPunct(); called directly here to
    // avoid linking the whole AbstractParser-Utils.cpp translation unit (and
    // its Vector/syntax dependency) into the small test binaries that only
    // need this one check.
    if (!charset::ascii::isPunct(c)) {
        return false;
    }
    setConfig().parser.prefixChar = c;
    emit sig_changed();
    return true;
}

bool ParserPageAdapter::getEncodeEmoji() const
{
    return getConfig().parser.encodeEmoji;
}

void ParserPageAdapter::setEncodeEmoji(const bool value)
{
    setConfig().parser.encodeEmoji = value;
    emit sig_changed();
}

bool ParserPageAdapter::getDecodeEmoji() const
{
    return getConfig().parser.decodeEmoji;
}

void ParserPageAdapter::setDecodeEmoji(const bool value)
{
    setConfig().parser.decodeEmoji = value;
    emit sig_changed();
}

void ParserPageAdapter::chooseRoomNameColor()
{
    if (!m_colorPicker) {
        qWarning() << "ParserPageAdapter::chooseRoomNameColor() called with no color picker set";
        return;
    }
    m_colorPicker(getRoomNameColor(), m_dialogParent, [this](QString ansiString) {
        setConfig().parser.roomNameColor = ansiString;
        emit sig_changed();
    });
}

void ParserPageAdapter::chooseRoomDescColor()
{
    if (!m_colorPicker) {
        qWarning() << "ParserPageAdapter::chooseRoomDescColor() called with no color picker set";
        return;
    }
    m_colorPicker(getRoomDescColor(), m_dialogParent, [this](QString ansiString) {
        setConfig().parser.roomDescColor = ansiString;
        emit sig_changed();
    });
}

void ParserPageAdapter::reload()
{
    emit sig_changed();
}
