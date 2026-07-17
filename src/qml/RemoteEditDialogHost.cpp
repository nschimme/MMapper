// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "RemoteEditDialogHost.h"

#include "../global/window_utils.h"
#include "../mpi/RemoteEditController.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QScreen>
#include <QStyle>
#include <QUrl>

RemoteEditDialogHost::RemoteEditDialogHost(RemoteEditController *const controller,
                                           QWidget *const parent)
    : QmlDialog(QString(), "RemoteEditDialog", parent)
    , m_controller(controller)
{
    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    mmqt::setWindowTitle2(*this,
                          QString("MMapper %1")
                              .arg(controller->getIsEditSession() ? "Editor" : "Viewer"),
                          controller->getTitle());

    // Matches RemoteEditWidget::minimumSizeHint() (QSize{100, 100}); QmlDialog
    // itself only ever grows the dialog to the QML root's implicit size (see
    // QmlDialog::setQmlSource()'s statusChanged handler), so the minimum has
    // to be set explicitly here.
    setMinimumSize(100, 100);

    setContextProperty("remoteEditController", controller);
    setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/RemoteEditDialog.qml")));

    // REVISIT: Restore geometry from config? (matches RemoteEditWidget's own REVISIT comment)
    if (QScreen *const screen = qApp->primaryScreen()) {
        setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                        Qt::AlignCenter,
                                        size(),
                                        screen->availableGeometry()));
    }

    setAttribute(Qt::WA_DeleteOnClose);

    show();
    raise();
    activateWindow();
}

RemoteEditDialogHost::~RemoteEditDialogHost() = default;

void RemoteEditDialogHost::closeEvent(QCloseEvent *const event)
{
    if (m_controller == nullptr) {
        QmlDialog::closeEvent(event);
        return;
    }

    if (m_controller->getSubmitted()) {
        event->accept();
        return;
    }

    if (m_controller->getIsEditSession()) {
        if (maybeDiscardAndCancel()) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        m_controller->cancel();
        event->accept();
    }
}

bool RemoteEditDialogHost::maybeDiscardAndCancel()
{
    if (m_controller->isDocumentDirty()) {
        QMessageBox dlg(this);
        dlg.setIcon(QMessageBox::Warning);
        dlg.setWindowTitle(m_controller->getTitle());
        dlg.setText(tr("You have edited the document.\n"
                       "Are you sure you want to discard all changes?"));
        dlg.setStandardButtons(QMessageBox::Discard | QMessageBox::Cancel);
        dlg.setDefaultButton(QMessageBox::Cancel);
        dlg.setEscapeButton(QMessageBox::Cancel);
        const int ret = dlg.exec();
        if (ret != QMessageBox::Discard) {
            return false;
        }
    }

    m_controller->cancel();
    return true;
}
