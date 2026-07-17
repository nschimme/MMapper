// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

// The MMAPPER_WITH_QML implementation of makeAnsiViewWindow(), declared in
// ../viewers/AnsiViewWindow.h. Mirrors how InfomarkEditController/RoomEdit-
// Controller's dialogs are wired up in mainwindow.cpp, but self-contained
// here (rather than in mainwindow.cpp) because makeAnsiViewWindow() is
// called from viewers/LaunchAsyncViewer.h and mpi/remoteeditwidget.cpp, not
// from MainWindow.

#include "../viewers/AnsiViewContent.h"
#include "../viewers/AnsiViewWindow.h"
#include "QmlDialog.h"

#include <QUrl>

std::unique_ptr<QDialog> makeAnsiViewWindow(const QString &program,
                                            const QString &title,
                                            const std::string_view body)
{
    auto *const dialog = new QmlDialog(QString(), "AnsiViewDialog", nullptr);
    auto *const content = new AnsiViewContent(program, title, body, dialog);
    dialog->setWindowTitle(content->getTitle());
    dialog->setContextProperty("ansiViewContent", content);
    dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/AnsiViewDialog.qml")));

    // AnsiViewWindow's constructor calls show()/raise()/activateWindow()
    // itself rather than waiting for the caller to do it (see
    // viewers/AnsiViewWindow.cpp); mirror that here so callers (Launch-
    // AsyncViewer.h's onSuccess(), RemoteEditWidget::slot_previewAnsi())
    // don't need to know which implementation they got back.
    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    return std::unique_ptr<QDialog>(dialog);
}
