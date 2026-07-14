#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QPointer>

class QWidget;

// Q_PROPERTY façade over Configuration::GroupManagerSettings (see
// ../configuration/configuration.h), mirroring grouppage.cpp's
// apply-on-change semantics. GroupManagerSettings has no ChangeMonitor, so
// reload() re-emits sig_changed unconditionally.
//
// Every setter (and the two color pickers) also emits sig_groupSettingsChanged,
// which PreferencesController forwards as its own signal of the same name —
// mirroring ConfigDialog's GroupPage::sig_groupSettingsChanged ->
// ConfigDialog::sig_groupSettingsChanged relay (see configdialog.cpp) so
// MainWindow can re-sync Mmapper2Group and QmlConfig the same way it does
// for the widget dialog.
class NODISCARD_QOBJECT GroupPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ getColor WRITE setColor NOTIFY sig_changed)
    Q_PROPERTY(QColor npcColor READ getNpcColor WRITE setNpcColor NOTIFY sig_changed)
    Q_PROPERTY(
        bool npcColorOverride READ getNpcColorOverride WRITE setNpcColorOverride NOTIFY sig_changed)
    Q_PROPERTY(bool npcSortBottom READ getNpcSortBottom WRITE setNpcSortBottom NOTIFY sig_changed)
    Q_PROPERTY(bool npcHide READ getNpcHide WRITE setNpcHide NOTIFY sig_changed)

private:
    // Parent for the native QColorDialog invoked by chooseColor()/
    // chooseNpcOverrideColor(); owned by PreferencesController, not by this
    // adapter.
    QPointer<QWidget> m_dialogParent;

public:
    explicit GroupPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD QColor getColor() const;
    void setColor(const QColor &value);

    NODISCARD QColor getNpcColor() const;
    void setNpcColor(const QColor &value);

    NODISCARD bool getNpcColorOverride() const;
    void setNpcColorOverride(bool value);

    NODISCARD bool getNpcSortBottom() const;
    void setNpcSortBottom(bool value);

    NODISCARD bool getNpcHide() const;
    void setNpcHide(bool value);

public:
    // Native QColorDialog pickers, mirroring GroupPage::slot_chooseColor()/
    // slot_chooseNpcOverrideColor().
    Q_INVOKABLE void chooseColor();
    Q_INVOKABLE void chooseNpcOverrideColor();

public slots:
    void reload();

signals:
    void sig_changed();
    void sig_groupSettingsChanged();
};
