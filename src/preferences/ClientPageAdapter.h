#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

// Q_PROPERTY façade over Configuration::IntegratedMudClientSettings (see
// ../configuration/configuration.h), mirroring clientpage.cpp's
// apply-on-change semantics. IntegratedMudClientSettings has no
// ChangeMonitor, so reload() re-emits sig_changed unconditionally.
//
// fontDisplayName mirrors ClientPage::updateFontAndColors()'s
// fontPushButton text ("<family> <style>, <size>"); the native QFontDialog
// is opened via chooseFont(), same as GroupPageAdapter/GraphicsPageAdapter's
// native QColorDialog pickers.
class NODISCARD_QOBJECT ClientPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString fontDisplayName READ getFontDisplayName NOTIFY sig_changed)
    Q_PROPERTY(
        QColor foregroundColor READ getForegroundColor WRITE setForegroundColor NOTIFY sig_changed)
    Q_PROPERTY(
        QColor backgroundColor READ getBackgroundColor WRITE setBackgroundColor NOTIFY sig_changed)

    Q_PROPERTY(int columns READ getColumns WRITE setColumns NOTIFY sig_changed)
    Q_PROPERTY(int rows READ getRows WRITE setRows NOTIFY sig_changed)
    Q_PROPERTY(int linesOfScrollback READ getLinesOfScrollback WRITE setLinesOfScrollback NOTIFY
                   sig_changed)
    Q_PROPERTY(int linesOfPeekPreview READ getLinesOfPeekPreview WRITE setLinesOfPeekPreview NOTIFY
                   sig_changed)
    Q_PROPERTY(int linesOfInputHistory READ getLinesOfInputHistory WRITE setLinesOfInputHistory
                   NOTIFY sig_changed)
    Q_PROPERTY(int tabCompletionDictionarySize READ getTabCompletionDictionarySize WRITE
                   setTabCompletionDictionarySize NOTIFY sig_changed)

    Q_PROPERTY(bool clearInputOnEnter READ getClearInputOnEnter WRITE setClearInputOnEnter NOTIFY
                   sig_changed)
    Q_PROPERTY(bool autoResizeTerminal READ getAutoResizeTerminal WRITE setAutoResizeTerminal NOTIFY
                   sig_changed)
    Q_PROPERTY(bool audibleBell READ getAudibleBell WRITE setAudibleBell NOTIFY sig_changed)
    Q_PROPERTY(bool visualBell READ getVisualBell WRITE setVisualBell NOTIFY sig_changed)
    Q_PROPERTY(bool useCommandSeparator READ getUseCommandSeparator WRITE setUseCommandSeparator
                   NOTIFY sig_changed)
    Q_PROPERTY(QString commandSeparator READ getCommandSeparator NOTIFY sig_changed)

private:
    // Parent for the native QFontDialog/QColorDialog invoked by
    // chooseFont()/chooseForegroundColor()/chooseBackgroundColor(); owned by
    // PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;

public:
    explicit ClientPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD QString getFontDisplayName() const;
    NODISCARD QColor getForegroundColor() const;
    void setForegroundColor(const QColor &value);
    NODISCARD QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor &value);

    NODISCARD int getColumns() const;
    void setColumns(int value);
    NODISCARD int getRows() const;
    void setRows(int value);
    NODISCARD int getLinesOfScrollback() const;
    void setLinesOfScrollback(int value);
    NODISCARD int getLinesOfPeekPreview() const;
    void setLinesOfPeekPreview(int value);
    NODISCARD int getLinesOfInputHistory() const;
    void setLinesOfInputHistory(int value);
    NODISCARD int getTabCompletionDictionarySize() const;
    void setTabCompletionDictionarySize(int value);

    NODISCARD bool getClearInputOnEnter() const;
    void setClearInputOnEnter(bool value);
    NODISCARD bool getAutoResizeTerminal() const;
    void setAutoResizeTerminal(bool value);
    NODISCARD bool getAudibleBell() const;
    void setAudibleBell(bool value);
    NODISCARD bool getVisualBell() const;
    void setVisualBell(bool value);
    NODISCARD bool getUseCommandSeparator() const;
    void setUseCommandSeparator(bool value);
    NODISCARD QString getCommandSeparator() const;
    // Mirrors CustomSeparatorValidator: exactly one printable, non-space,
    // non-backslash character. Returns false (config untouched) otherwise.
    Q_INVOKABLE bool setCommandSeparator(const QString &value);

public:
    Q_INVOKABLE void chooseFont();
    Q_INVOKABLE void chooseForegroundColor();
    Q_INVOKABLE void chooseBackgroundColor();

public slots:
    void reload();

signals:
    void sig_changed();
};
