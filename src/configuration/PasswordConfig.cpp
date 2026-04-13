// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "PasswordConfig.h"

#include "../global/macros.h"

#ifndef MMAPPER_NO_QTKEYCHAIN
static const QLatin1String APP_NAME("org.mume.mmapper");
#endif

PasswordConfig::PasswordConfig(QObject *const parent)
    : QObject(parent)
#ifndef MMAPPER_NO_QTKEYCHAIN
    , m_readJob(APP_NAME)
    , m_writeJob(APP_NAME)
    , m_deleteJob(APP_NAME)
{
    m_readJob.setAutoDelete(false);
    m_writeJob.setAutoDelete(false);
    m_deleteJob.setAutoDelete(false);

    connect(&m_readJob, &QKeychain::ReadPasswordJob::finished, [this]() {
        if (m_readJob.error()) {
            emit sig_error(m_readJob.errorString());
        } else {
            emit sig_incomingPassword(m_readJob.textData());
        }
    });

    connect(&m_writeJob, &QKeychain::WritePasswordJob::finished, [this]() {
        if (m_writeJob.error()) {
            emit sig_error(m_writeJob.errorString());
        }
    });

    connect(&m_deleteJob, &QKeychain::DeletePasswordJob::finished, [this]() {
        if (m_deleteJob.error()) {
            emit sig_error(m_deleteJob.errorString());
        } else {
            emit sig_passwordDeleted();
        }
    });
}
#else
{
}
#endif

void PasswordConfig::setPassword(const QString &accountName, const QString &password)
{
#ifndef MMAPPER_NO_QTKEYCHAIN
    m_writeJob.setKey(accountName);
    m_writeJob.setTextData(password);
    m_writeJob.start();
#else
    std::ignore = accountName;
    std::ignore = password;
    emit sig_error("Password setting is not available.");
#endif
}

void PasswordConfig::getPassword(const QString &accountName)
{
#ifndef MMAPPER_NO_QTKEYCHAIN
    m_readJob.setKey(accountName);
    m_readJob.start();
#else
    std::ignore = accountName;
    emit sig_error("Password retrieval is not available.");
#endif
}

void PasswordConfig::deletePassword(const QString &accountName)
{
#ifndef MMAPPER_NO_QTKEYCHAIN
    m_deleteJob.setKey(accountName);
    m_deleteJob.start();
#else
    std::ignore = accountName;
    emit sig_error("Password deletion is not available.");
#endif
}
