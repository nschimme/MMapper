// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "remoteeditsession.h"

#include "../global/utils.h"
#include "remoteedit.h"

#ifdef MMAPPER_WITH_QML
#include "../qml/RemoteEditDialogHost.h"
#include "RemoteEditController.h"
#else
#include "remoteeditwidget.h"
#endif

#ifndef Q_OS_WASM
#include "remoteeditprocess.h"
#endif

#include <cassert>

#include <QMessageLogContext>
#include <QObject>
#include <QPointer>
#include <QString>

RemoteEditSession::RemoteEditSession(const RemoteInternalId internalId,
                                     const RemoteSessionId sessionId,
                                     RemoteEdit *const remoteEdit)
    : QObject(remoteEdit)
    , m_manager(remoteEdit)
    , m_internalId(internalId)
    , m_sessionId(sessionId)
{
    assert(m_manager != nullptr);
}

void RemoteEditSession::save()
{
    m_manager->save(this);
}

void RemoteEditSession::cancel()
{
    m_manager->cancel(this);
}

#ifdef MMAPPER_WITH_QML
RemoteEditInternalSession::RemoteEditInternalSession(const RemoteInternalId internalId,
                                                     const RemoteSessionId sessionId,
                                                     const QString &title,
                                                     const QString &body,
                                                     RemoteEdit *const parent)
    : RemoteEditSession(internalId, sessionId, parent)
    , m_controller(new RemoteEditController(isEditSession(), title, body))
    , m_dialog(new RemoteEditDialogHost(m_controller,
                                        checked_dynamic_downcast<QWidget *>(
                                            parent->parent()) // MainWindow
                                        ))
{
    m_controller->setParent(m_dialog.data());
    connect(m_controller, &RemoteEditController::sig_save, this, &RemoteEditSession::slot_onSave);
    connect(m_controller,
            &RemoteEditController::sig_cancel,
            this,
            &RemoteEditSession::slot_onCancel);
}

RemoteEditInternalSession::~RemoteEditInternalSession()
{
    qDebug() << "Destructed RemoteEditInternalSession" << getInternalId().asUint32()
             << getSessionId().asInt32();
    if (auto *const p = m_dialog.get()) {
        p->close();
    }
}
#else
RemoteEditInternalSession::RemoteEditInternalSession(const RemoteInternalId internalId,
                                                     const RemoteSessionId sessionId,
                                                     const QString &title,
                                                     const QString &body,
                                                     RemoteEdit *const parent)
    : RemoteEditSession(internalId, sessionId, parent)
    , m_widget(
          new RemoteEditWidget(isEditSession(),
                               title,
                               body,
                               checked_dynamic_downcast<QWidget *>(parent->parent()) // MainWindow
                               ))
{
    const auto widget = m_widget.data();
    connect(widget, &RemoteEditWidget::sig_save, this, &RemoteEditSession::slot_onSave);
    connect(widget, &RemoteEditWidget::sig_cancel, this, &RemoteEditSession::slot_onCancel);
}

RemoteEditInternalSession::~RemoteEditInternalSession()
{
    qDebug() << "Destructed RemoteEditInternalSession" << getInternalId().asUint32()
             << getSessionId().asInt32();
    if (auto *const p = m_widget.get()) {
        p->close();
    }
}
#endif

#ifndef Q_OS_WASM
RemoteEditExternalSession::RemoteEditExternalSession(const RemoteInternalId internalId,
                                                     const RemoteSessionId sessionId,
                                                     const QString &title,
                                                     const QString &body,
                                                     RemoteEdit *const parent)
    : RemoteEditSession(internalId, sessionId, parent)
    , m_process(new RemoteEditProcess(isEditSession(), title, body, this))
{
    const auto proc = m_process.data();
    connect(proc, &RemoteEditProcess::sig_save, this, &RemoteEditExternalSession::slot_onSave);
    connect(proc, &RemoteEditProcess::sig_cancel, this, &RemoteEditExternalSession::slot_onCancel);
}

RemoteEditExternalSession::~RemoteEditExternalSession()
{
    qDebug() << "Destructed RemoteEditExternalSession" << getInternalId().asUint32()
             << getSessionId().asInt32();
    if (auto *const p = m_process.get()) {
        p->deleteLater();
    }
}
#endif
