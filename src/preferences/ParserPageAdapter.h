#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

// Q_PROPERTY façade over Configuration::ParserSettings (see
// ../configuration/configuration.h), mirroring parserpage.cpp's
// apply-on-change semantics. ParserSettings has no ChangeMonitor, so
// reload() re-emits sig_changed unconditionally.
//
// roomNameColor/roomDescColor are raw ANSI escape-sequence strings (e.g.
// "[1;32m"), same representation ParserSettings stores; QML never parses
// them directly. Instead of porting AnsiColorDialog's fg/bg AnsiCombo +
// bold/italic/underline UI to QML (Option A), this adapter keeps
// AnsiColorDialog as a native QDialog launched via the chooseX() invokables
// (Option B) — AnsiColorDialog is a small, well-tested, currently-widget-only
// dialog, and re-deriving its 16-color swatch tables and live-preview logic
// in QML would be a large surface for no behavioral gain in this round.
// roomNameColorPreview/roomDescColorPreview expose a QColor pair (fg/bg) so
// the QML page can still render an in-page swatch, mirroring
// AnsiCombo::makeWidgetColoured()'s palette-based preview.
class NODISCARD_QOBJECT ParserPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString roomNameColor READ getRoomNameColor NOTIFY sig_changed)
    Q_PROPERTY(QString roomDescColor READ getRoomDescColor NOTIFY sig_changed)
    Q_PROPERTY(QColor roomNameColorFg READ getRoomNameColorFg NOTIFY sig_changed)
    Q_PROPERTY(QColor roomNameColorBg READ getRoomNameColorBg NOTIFY sig_changed)
    Q_PROPERTY(QColor roomDescColorFg READ getRoomDescColorFg NOTIFY sig_changed)
    Q_PROPERTY(QColor roomDescColorBg READ getRoomDescColorBg NOTIFY sig_changed)

    Q_PROPERTY(QString prefixChar READ getPrefixChar NOTIFY sig_changed)
    Q_PROPERTY(bool encodeEmoji READ getEncodeEmoji WRITE setEncodeEmoji NOTIFY sig_changed)
    Q_PROPERTY(bool decodeEmoji READ getDecodeEmoji WRITE setDecodeEmoji NOTIFY sig_changed)

private:
    // Parent for the native AnsiColorDialog invoked by the chooseX()
    // invokables; owned by PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;

public:
    explicit ParserPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD QString getRoomNameColor() const;
    NODISCARD QString getRoomDescColor() const;
    NODISCARD QColor getRoomNameColorFg() const;
    NODISCARD QColor getRoomNameColorBg() const;
    NODISCARD QColor getRoomDescColorFg() const;
    NODISCARD QColor getRoomDescColorBg() const;

    NODISCARD QString getPrefixChar() const;
    // Mirrors CommandPrefixValidator's single-character isValidPrefix()
    // check (see parserpage.cpp, ../parser/AbstractParser-Utils.cpp); returns
    // false (and leaves the config untouched) if value isn't exactly one
    // valid-prefix Latin-1 character.
    Q_INVOKABLE bool setPrefixChar(const QString &value);

    NODISCARD bool getEncodeEmoji() const;
    void setEncodeEmoji(bool value);
    NODISCARD bool getDecodeEmoji() const;
    void setDecodeEmoji(bool value);

public:
    // Native AnsiColorDialog pickers, mirroring
    // ParserPage::slot_roomNameColorClicked()/slot_roomDescColorClicked().
    Q_INVOKABLE void chooseRoomNameColor();
    Q_INVOKABLE void chooseRoomDescColor();

public slots:
    void reload();

signals:
    void sig_changed();
};
