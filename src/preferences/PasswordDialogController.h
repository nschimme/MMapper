#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QString>

// Widget-free QML port of preferences/ManagePasswordDialog.{h,cpp,ui} (kept
// under NOT WITH_QML). Owns no widgets: it's a plain data holder for the
// account name / password fields plus the platform-dependent
// showPasswordControls flag, mirroring InfomarkEditController/AnsiColor-
// PickerController's role for their own dialogs. See qml/PasswordDialog.qml
// for the QML side and qml/PasswordDialogLauncher.h for the MMAPPER_WITH_QML
// construction seam (this class has no QmlDialog dependency of its own, so
// it compiles cleanly into TestMainWindow/TestQml, same as
// ParserPageAdapter's ColorPicker split -- see ParserPageAdapter.h's
// ColorPicker doc comment).
//
// QML contract (stub-drift guard: keep TestQml.cpp's usage of the real
// controller in sync with this surface):
//   Q_PROPERTY QString accountName -- read/write, bound to the account-name
//     TextField's text.
//   Q_PROPERTY QString password -- read/write, bound to the password
//     TextField's text; only meaningful when showPasswordControls is true.
//   Q_PROPERTY bool hasStoredPassword -- NOTIFY sig_changed; drives the
//     Delete button's enabled state. Set once at construction from whether
//     the caller supplied an existing password, then cleared by
//     requestDelete() (mirrors ManagePasswordDialog's deleteButton handler
//     clearing the password field and disabling itself after emitting
//     sig_deleteRequested()).
//   Q_PROPERTY bool showPasswordControls -- CONSTANT; false only on WASM
//     (mirrors ManagePasswordDialog::ManagePasswordDialog()'s
//     CURRENT_PLATFORM == PlatformEnum::Wasm branch, which hides the
//     password field, View toggle, and Delete button and retitles the
//     dialog to "Manage Account" -- the title is handled by the launcher,
//     not this controller).
//   Q_PROPERTY QString errorText -- NOTIFY sig_changed; empty/invisible
//     unless something sets it via setErrorText() (inline-error surface for
//     future use; GeneralPageAdapter's keychain errors still route through
//     the native QMessageBox, unchanged by this port).
//   Q_INVOKABLE requestDelete() -- mirrors ManagePasswordDialog's
//     deleteButton click handler: emits sig_deleteRequested(), then clears
//     password and hasStoredPassword locally.
// Signals: sig_changed(), sig_deleteRequested().
class NODISCARD_QOBJECT PasswordDialogController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString accountName READ getAccountName WRITE setAccountName NOTIFY sig_changed)
    Q_PROPERTY(QString password READ getPassword WRITE setPassword NOTIFY sig_changed)
    Q_PROPERTY(bool hasStoredPassword READ getHasStoredPassword NOTIFY sig_changed)
    Q_PROPERTY(bool showPasswordControls READ getShowPasswordControls CONSTANT)
    Q_PROPERTY(QString errorText READ getErrorText WRITE setErrorText NOTIFY sig_changed)

private:
    QString m_accountName;
    QString m_password;
    bool m_hasStoredPassword = false;
    const bool m_showPasswordControls;
    QString m_errorText;

public:
    explicit PasswordDialogController(QString accountName,
                                      QString password,
                                      bool hasStoredPassword,
                                      bool showPasswordControls,
                                      QObject *parent);

public:
    NODISCARD QString getAccountName() const { return m_accountName; }
    void setAccountName(const QString &value);
    NODISCARD QString getPassword() const { return m_password; }
    void setPassword(const QString &value);
    NODISCARD bool getHasStoredPassword() const { return m_hasStoredPassword; }
    NODISCARD bool getShowPasswordControls() const { return m_showPasswordControls; }
    NODISCARD QString getErrorText() const { return m_errorText; }
    void setErrorText(const QString &value);

    Q_INVOKABLE void requestDelete();

signals:
    void sig_changed();
    void sig_deleteRequested();
};
