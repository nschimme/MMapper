// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "PasswordDialogController.h"

#include <utility>

PasswordDialogController::PasswordDialogController(QString accountName,
                                                   QString password,
                                                   const bool hasStoredPassword,
                                                   const bool showPasswordControls,
                                                   QObject *const parent)
    : QObject(parent)
    , m_accountName(std::move(accountName))
    , m_password(std::move(password))
    , m_hasStoredPassword(hasStoredPassword)
    , m_showPasswordControls(showPasswordControls)
{}

void PasswordDialogController::setAccountName(const QString &value)
{
    if (m_accountName == value) {
        return;
    }
    m_accountName = value;
    emit sig_changed();
}

void PasswordDialogController::setPassword(const QString &value)
{
    if (m_password == value) {
        return;
    }
    m_password = value;
    emit sig_changed();
}

void PasswordDialogController::setErrorText(const QString &value)
{
    if (m_errorText == value) {
        return;
    }
    m_errorText = value;
    emit sig_changed();
}

void PasswordDialogController::requestDelete()
{
    emit sig_deleteRequested();
    m_password.clear();
    m_hasStoredPassword = false;
    emit sig_changed();
}
