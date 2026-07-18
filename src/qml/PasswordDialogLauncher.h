#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <functional>

#include <QString>

class QWidget;

// The MMAPPER_WITH_QML replacement for preferences/ManagePasswordDialog
// (kept under NOT WITH_QML): constructs a QmlDialog hosting
// PasswordDialog.qml, driven by a fresh PasswordDialogController (see
// ../preferences/PasswordDialogController.h), and invokes onAccepted ONLY on
// accept -- mirroring GeneralPageAdapter's two ManagePasswordDialog
// construction sites exactly (see GeneralPageAdapter.cpp).
//
// Declared in its own widget-free header (rather than as a static method on
// PasswordDialogController, which is compiled into TestMainWindow/TestQml
// without Qt6::Quick linked) so preferences/GeneralPageAdapter.cpp -- also
// compiled directly into both test binaries -- never has to reference this
// symbol itself; MainWindow wires it in via
// GeneralPageAdapter::setPasswordDialogLauncher() instead (see
// mainwindow.cpp's slot_onPreferences()), the same seam
// ParserPageAdapter::setColorPicker()/qml/AnsiColorPickerLauncher.h use.
namespace password_dialog {
void manage(const QString &accountName,
            const QString &password,
            bool hasStoredPassword,
            QWidget *parent,
            std::function<void(const QString &accountName, const QString &password)> onAccepted,
            std::function<void()> onDeleteRequested);
} // namespace password_dialog
