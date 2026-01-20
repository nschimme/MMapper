// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "remoteedit.h"

#include "../configuration/configuration.h"
#include "../global/Consts.h"
#include "remoteeditsession.h"

#include <cassert>
#include <memory>
#include <utility>

#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMessageBox>
#include <QMessageLogContext>
#include <QString>

using char_consts::C_NEWLINE;

RemoteEdit::RemoteEdit(QObject *const parent)
    : QObject(parent)
{}

void RemoteEdit::slot_remoteView(const QString &title, const QString &body)
{
    addSession(REMOTE_VIEW_SESSION_ID, title, body);
}

void RemoteEdit::slot_remoteEdit(const RemoteSessionId sessionId,
                                 const QString &title,
                                 const QString &body)
{
    addSession(sessionId, title, body);
}

void RemoteEdit::startInternalEdit(const QString &title,
                                   const QString &body,
                                   std::function<void(QString)> onSave,
                                   std::function<void()> onCancel)
{
    addSession(REMOTE_INTERNAL_EDIT_SESSION_ID, title, body);
    const auto internalId = RemoteInternalId{m_greatestUsedId};
    const auto search = m_sessions.find(internalId);
    if (search != m_sessions.end()) {
        search->second->m_onSave = std::move(onSave);
        search->second->m_onCancel = std::move(onCancel);
    }
}

void RemoteEdit::startInternalView(const QString &title, const QString &body)
{
    addSession(REMOTE_VIEW_SESSION_ID, title, body);
}

void RemoteEdit::addSession(const RemoteSessionId sessionId,
                            const QString &title,
                            const QString &body)
{
    const auto internalId = RemoteInternalId{getInternalIdCount()};
    std::unique_ptr<RemoteEditSession> session;

    if (getConfig().mumeClientProtocol.internalRemoteEditor) {
        session = std::make_unique<RemoteEditInternalSession>(internalId,
                                                              sessionId,
                                                              title,
                                                              body,
                                                              this);
    } else {
#ifndef Q_OS_WASM
        session = std::make_unique<RemoteEditExternalSession>(internalId,
                                                              sessionId,
                                                              title,
                                                              body,
                                                              this);
#else
        QMessageBox::information(nullptr,
                                 "External Editor Not Supported",
                                 "Editing in an external editor is not supported on this platform.");
        return;
#endif
    }
    m_sessions.insert(std::make_pair(internalId, std::move(session)));

    m_greatestUsedId = internalId.asUint32(); // Increment internalId counter
}

void RemoteEdit::removeSession(const RemoteEditSession &session)
{
    const auto internalId = session.getInternalId();
    const auto search = m_sessions.find(internalId);
    if (search != m_sessions.end()) {
        qDebug() << "Destroying RemoteEditSession" << internalId.asUint32();
        m_sessions.erase(search);

    } else {
        qWarning() << "Unable to find" << internalId.asUint32() << "session to erase";
    }
}

void RemoteEdit::cancel(const RemoteEditSession *const pSession)
{
    auto &session = deref(pSession);

    if (session.m_onCancel) {
        session.m_onCancel();
    } else if (session.isEditSession() && session.isConnected()) {
        qDebug() << "Cancelling session" << session.getSessionId().asInt32();
        emit sig_remoteEditCancel(session.getSessionId());
    }

    removeSession(session);
}

void RemoteEdit::save(const RemoteEditSession *const pSession)
{
    auto &session = deref(pSession);
    if (session.m_onSave) {
        session.m_onSave(session.getContent());
    } else {
        trySave(session);
    }
    removeSession(session);
}

void RemoteEdit::trySave(const RemoteEditSession &session)
{
    if (!session.isEditSession()) {
        qWarning() << "Session" << session.getInternalId().asUint32()
                   << "was not an edit session and could not be saved";
        assert(false);
        return;
    }

    // Submit the edit session if we are still connected
    if (!session.isConnected()) {
        trySaveLocally(session);
    } else {
        sendToMume(session);
    }
}

void RemoteEdit::sendToMume(const RemoteEditSession &session)
{
    if (!session.isEditSession()) {
        std::abort();
    }

    qDebug() << "Saving session" << session.getSessionId().asInt32();
    // REVISIT: should we warn if this transformation modifies the content
    // (e.g. unicode transliteration, etc).
    auto latin1 = Latin1Bytes{
        mmqt::toQByteArrayLatin1(session.getContent())}; // MPI is always Latin1
    emit sig_remoteEditSave(session.getSessionId(), latin1);
}

void RemoteEdit::trySaveLocally(const RemoteEditSession &session)
{
    if (!session.isEditSession()) {
        assert(false);
    }

    QMessageBox dlg(
        QMessageBox::Critical,
        "MUME Disconnected",
        "The connection to MUME was lost. Your unsaved changes will be lost unless you save the file locally now.",
        QMessageBox::StandardButtons{QMessageBox::Save | QMessageBox::Discard
                                     | QMessageBox::Cancel});
    const auto id = session.getInternalId().asUint32();
    const auto body = session.getContent().toUtf8();
    if (dlg.exec() == QMessageBox::Save) {
        qDebug() << "Session" << id << "was saved";
        QFileDialog::saveFileContent(body, QString("MMapper-Edit-%1.txt").arg(id));
    }
    QGuiApplication::clipboard()->setText(body);
    qWarning() << "Session" << id << "was copied to the clipboard";
}

void RemoteEdit::onDisconnected()
{
    for (const auto &pair : m_sessions) {
        const auto &id = pair.first;
        const auto &session = pair.second;
        if (session->isEditSession() && !session->m_onSave) {
            qWarning() << "Session" << id.asUint32() << "marked as disconnected";
            session->setDisconnected();
        }
    }
}
