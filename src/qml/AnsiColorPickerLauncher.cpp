// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnsiColorPickerLauncher.h"

#include "../mainwindow/AnsiColorPickerController.h"
#include "QmlDialog.h"

#include <QDialog>
#include <QUrl>

void ansi_color_picker::getColor(const QString &ansiString,
                                 QWidget *const parent,
                                 std::function<void(QString)> callback)
{
    auto *const dialog = new QmlDialog(QObject::tr("Ansi Color Dialog"),
                                       "AnsiColorPickerDialog",
                                       parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *const controller = new AnsiColorPickerController(dialog);
    controller->init(ansiString);

    dialog->setContextProperty("ansiColorPickerController", controller);
    dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/AnsiColorPickerDialog.qml")));

    QObject::connect(dialog,
                     &QDialog::finished,
                     dialog,
                     [dialog, controller, callback](const int result) {
                         if (result == QDialog::Accepted) {
                             callback(controller->getResultAnsiString());
                         }
                     });
    dialog->open();
}
