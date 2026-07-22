#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

// Q_PROPERTY façade over Configuration::AutoLogSettings (see
// ../configuration/configuration.h), mirroring autologpage.cpp's
// apply-on-change semantics. AutoLogSettings has no ChangeMonitor, so
// reload() re-emits sig_changed unconditionally.
//
// cleanupStrategy is exposed as an int matching AutoLoggerEnum's ordinal
// (KeepForever=0, DeleteDays=1, DeleteSize=2; see
// ../global/ConfigEnums.h) so QML's ButtonGroup can bind a plain integer
// without needing the enum registered as a QML type.
//
// rotateWhenLogsReachMB / deleteWhenLogsReachMB convert the underlying
// byte counts to/from megabytes using the same MEGABYTE_IN_BYTES=1000000
// factor autologpage.cpp uses.
class NODISCARD_QOBJECT AutoLogPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool autoLog READ getAutoLog WRITE setAutoLog NOTIFY sig_changed)
    Q_PROPERTY(QString autoLogDirectory READ getAutoLogDirectory WRITE setAutoLogDirectory NOTIFY
                   sig_changed)
    Q_PROPERTY(int rotateWhenLogsReachMB READ getRotateWhenLogsReachMB WRITE
                   setRotateWhenLogsReachMB NOTIFY sig_changed)
    Q_PROPERTY(
        int cleanupStrategy READ getCleanupStrategy WRITE setCleanupStrategy NOTIFY sig_changed)
    Q_PROPERTY(int deleteWhenLogsReachDays READ getDeleteWhenLogsReachDays WRITE
                   setDeleteWhenLogsReachDays NOTIFY sig_changed)
    Q_PROPERTY(int deleteWhenLogsReachMB READ getDeleteWhenLogsReachMB WRITE
                   setDeleteWhenLogsReachMB NOTIFY sig_changed)
    Q_PROPERTY(bool askDelete READ getAskDelete WRITE setAskDelete NOTIFY sig_changed)
    // True when running under WebAssembly, where the native "choose log
    // location" directory dialog does not exist; mirrors autologpage.cpp:62
    // (Select button disabled under CURRENT_PLATFORM == PlatformEnum::Wasm).
    Q_PROPERTY(bool isWasm READ getIsWasm CONSTANT)

private:
    // Parent for the native QFileDialog invoked by browseForDirectory();
    // owned by PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;

public:
    explicit AutoLogPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD bool getAutoLog() const;
    void setAutoLog(bool value);

    NODISCARD QString getAutoLogDirectory() const;
    void setAutoLogDirectory(const QString &value);

    NODISCARD int getRotateWhenLogsReachMB() const;
    void setRotateWhenLogsReachMB(int value);

    NODISCARD int getCleanupStrategy() const;
    void setCleanupStrategy(int value);

    NODISCARD int getDeleteWhenLogsReachDays() const;
    void setDeleteWhenLogsReachDays(int value);

    NODISCARD int getDeleteWhenLogsReachMB() const;
    void setDeleteWhenLogsReachMB(int value);

    NODISCARD bool getAskDelete() const;
    void setAskDelete(bool value);

    NODISCARD static bool getIsWasm();

public:
    // Opens a native "choose log location" directory dialog and, if a
    // directory was chosen, updates the property/config and emits
    // sig_changed.
    Q_INVOKABLE void browseForDirectory();

public slots:
    void reload();

signals:
    void sig_changed();
};
