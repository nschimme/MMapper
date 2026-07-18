// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "PasswordDialogLauncher.h"

#include "../global/ConfigConsts-Computed.h"
#include "../global/ConfigEnums.h"
#include "../preferences/PasswordDialogController.h"
#include "QmlDialog.h"

#include <QDialog>
#include <QUrl>

void password_dialog::manage(
    const QString &accountName,
    const QString &password,
    const bool hasStoredPassword,
    QWidget *const parent,
    std::function<void(const QString &accountName, const QString &password)> onAccepted,
    std::function<void()> onDeleteRequested)
{
    // Mirrors ManagePasswordDialog::ManagePasswordDialog()'s WASM branch:
    // hide password/view/delete and retitle to "Manage Account".
    const bool showPasswordControls = CURRENT_PLATFORM != PlatformEnum::Wasm;
    const QString title = showPasswordControls ? QObject::tr("Manage Password")
                                               : QObject::tr("Manage Account");

    auto *const dialog = new QmlDialog(title, "PasswordDialog", parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *const controller = new PasswordDialogController(accountName,
                                                          password,
                                                          hasStoredPassword,
                                                          showPasswordControls,
                                                          dialog);

    dialog->setContextProperty("passwordDialogController", controller);
    dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/PasswordDialog.qml")));

    QObject::connect(controller,
                     &PasswordDialogController::sig_deleteRequested,
                     dialog,
                     [onDeleteRequested]() { onDeleteRequested(); });

    QObject::connect(dialog,
                     &QDialog::finished,
                     dialog,
                     [dialog, controller, onAccepted](const int result) {
                         if (result == QDialog::Accepted) {
                             onAccepted(controller->getAccountName(), controller->getPassword());
                         }
                     });

    dialog->open();
}
