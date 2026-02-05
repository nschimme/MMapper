#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../global/macros.h"
#include <QObject>
#include <QString>

class NODISCARD_QOBJECT ParserPageViewModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString roomNameColor READ roomNameColor WRITE setRoomNameColor NOTIFY settingsChanged)
    Q_PROPERTY(QString roomDescColor READ roomDescColor WRITE setRoomDescColor NOTIFY settingsChanged)
    Q_PROPERTY(QString prefixChar READ prefixChar WRITE setPrefixChar NOTIFY settingsChanged)
    Q_PROPERTY(bool encodeEmoji READ encodeEmoji WRITE setEncodeEmoji NOTIFY settingsChanged)
    Q_PROPERTY(bool decodeEmoji READ decodeEmoji WRITE setDecodeEmoji NOTIFY settingsChanged)

public:
    explicit ParserPageViewModel(QObject *parent = nullptr);

    NODISCARD QString roomNameColor() const; void setRoomNameColor(const QString &v);
    NODISCARD QString roomDescColor() const; void setRoomDescColor(const QString &v);
    NODISCARD QString prefixChar() const; void setPrefixChar(const QString &v);
    NODISCARD bool encodeEmoji() const; void setEncodeEmoji(bool v);
    NODISCARD bool decodeEmoji() const; void setDecodeEmoji(bool v);

    void loadConfig();

signals:
    void settingsChanged();
};
