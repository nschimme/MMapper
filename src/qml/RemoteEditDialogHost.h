#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "QmlDialog.h"

class RemoteEditController;
class QCloseEvent;

// QmlDialog host for RemoteEditDialog.qml, the QML port of RemoteEditWidget
// (see ../mpi/remoteeditwidget.h). This step only adds the class; nothing in
// the session layer (mpi/remoteeditsession.cpp/remoteedit.cpp) constructs it
// yet, so RemoteEditWidget remains the active UI.
//
// closeEvent() ports RemoteEditWidget::closeEvent()/slot_maybeCancel() move
// for move: the window-manager close path ([X], Alt+F4) is guarded here;
// the menu's Submit/Exit actions instead call controller->submit()/cancel()
// then QDialog::close() themselves (see RemoteEditDialog.qml), which re-
// enters this same closeEvent() but with getSubmitted() already true, so it
// accepts immediately -- exactly mirroring how RemoteEditWidget::
// slot_finishEdit()/slot_cancelEdit() set m_submitted before calling
// close().
class NODISCARD_QOBJECT RemoteEditDialogHost final : public QmlDialog
{
    Q_OBJECT

private:
    RemoteEditController *m_controller = nullptr;

public:
    explicit RemoteEditDialogHost(RemoteEditController *controller, QWidget *parent);
    ~RemoteEditDialogHost() final;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // Returns true if it's OK to proceed with closing (nothing to lose, or
    // the user confirmed discarding); false means the close was cancelled.
    // Ports RemoteEditWidget::slot_maybeCancel().
    NODISCARD bool maybeDiscardAndCancel();
};
