// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ParserPageViewModel.h"
#include "../configuration/configuration.h"

ParserPageViewModel::ParserPageViewModel(QObject *p) : QObject(p) {}

QString ParserPageViewModel::roomNameColor() const { return getConfig().parser.roomNameColor; }
void ParserPageViewModel::setRoomNameColor(const QString &v) { if (getConfig().parser.roomNameColor != v) { setConfig().parser.roomNameColor = v; emit settingsChanged(); } }
QString ParserPageViewModel::roomDescColor() const { return getConfig().parser.roomDescColor; }
void ParserPageViewModel::setRoomDescColor(const QString &v) { if (getConfig().parser.roomDescColor != v) { setConfig().parser.roomDescColor = v; emit settingsChanged(); } }
QString ParserPageViewModel::prefixChar() const { return QString(getConfig().parser.prefixChar); }
void ParserPageViewModel::setPrefixChar(const QString &v) { if (!v.isEmpty() && getConfig().parser.prefixChar != v[0].toLatin1()) { setConfig().parser.prefixChar = v[0].toLatin1(); emit settingsChanged(); } }
bool ParserPageViewModel::encodeEmoji() const { return getConfig().parser.encodeEmoji; }
void ParserPageViewModel::setEncodeEmoji(bool v) { if (getConfig().parser.encodeEmoji != v) { setConfig().parser.encodeEmoji = v; emit settingsChanged(); } }
bool ParserPageViewModel::decodeEmoji() const { return getConfig().parser.decodeEmoji; }
void ParserPageViewModel::setDecodeEmoji(bool v) { if (getConfig().parser.decodeEmoji != v) { setConfig().parser.decodeEmoji = v; emit settingsChanged(); } }

void ParserPageViewModel::loadConfig() { emit settingsChanged(); }
