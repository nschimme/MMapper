#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <functional>

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
// them directly.
//
// The fg/bg/bold/italic/underline picker itself is now ported to QML (see
// qml/AnsiColorPickerDialog.qml + mainwindow/AnsiColorPickerController.h),
// but chooseRoomNameColor()/chooseRoomDescColor() below can't construct a
// QmlDialog directly: this .cpp is compiled straight into TestMainWindow
// (see tests/CMakeLists.txt's preferences_adapter_SRCS), which links neither
// Qt6::Quick nor mm_qml. Instead, the actual dialog-construction is injected
// via setColorPicker() -- MainWindow wires it to
// qml/AnsiColorPickerLauncher.h's ansi_color_picker::getColor() in
// slot_onPreferences() (a file that's only ever compiled into the real
// "mmapper" binary) -- and defaults to the native AnsiColorDialog (kept
// under NOT WITH_QML) so this class still behaves correctly if nothing calls
// setColorPicker(), same as the WITH_QML=OFF build does today.
// roomNameColorPreview/roomDescColorPreview expose a QColor pair (fg/bg) so
// the QML page can still render an in-page swatch, mirroring
// AnsiCombo::makeWidgetColoured()'s palette-based preview.
class NODISCARD_QOBJECT ParserPageAdapter final : public QObject
{
    Q_OBJECT

public:
    using ColorPicker = std::function<
        void(const QString &ansiString, QWidget *parent, std::function<void(QString)> callback)>;

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
    // Parent for the color-picker dialog invoked by the chooseX()
    // invokables; owned by PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;
    ColorPicker m_colorPicker;

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
    // Overrides the default (native AnsiColorDialog) picker; see the
    // ColorPicker/chooseX() doc comment above. Not Q_INVOKABLE: only
    // MainWindow (C++) calls this, never QML.
    void setColorPicker(ColorPicker picker) { m_colorPicker = std::move(picker); }

    // Color pickers, mirroring
    // ParserPage::slot_roomNameColorClicked()/slot_roomDescColorClicked().
    Q_INVOKABLE void chooseRoomNameColor();
    Q_INVOKABLE void chooseRoomDescColor();

public slots:
    void reload();

signals:
    void sig_changed();
};
