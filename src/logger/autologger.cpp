// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include "autologger.h"

#include "../configuration/configuration.h"
#include "../parser/parserutils.h"

static constexpr const int SESSION_STR_LENGTH = 5;

AutoLogger::AutoLogger(QObject *const parent)
    : QObject(parent)
    , m_sessionString{generateSessionString(SESSION_STR_LENGTH)}
    , m_title{generateTitle()}
{}

AutoLogger::~AutoLogger()
{
    m_logFile.flush();
    m_logFile.close();
}

bool AutoLogger::createFile()
{
    if (m_logFile.is_open())
        m_logFile.close();

    auto fileMode = std::fstream::out | std::fstream::app;
    if (m_curFile >= getConfig().autoLog.autoLogMaxFiles) {
        // Wrap around and start overwriting logs.
        fileMode = std::fstream::out;
        m_curFile = 0;
    }

    QString fileName = QString(m_title + "_" + QString::number(m_curFile) + ".txt");
    m_logFile.open(fileName.toStdString(), fileMode);
    if (!m_logFile.is_open()) // Could not create file.
        return false;

    m_curLines = 0;
    m_curFile++;

    return true;
}

bool AutoLogger::writeLine(const QByteArray &ba)
{
    if (!m_shouldLog || !getConfig().autoLog.autoLog)
        return false;

    if (!m_logFile.is_open()) {
        if (!createFile())
            return false; // Could not create the log file.
    }

    QString str = QString::fromLatin1(ba);
    if (str.contains('\x1b'))
        ParserUtils::removeAnsiMarksInPlace(str);

    if (m_curLines > getConfig().autoLog.autoLogMaxLines) {
        m_logFile.close();
        if (!createFile())
            return false;
    }

    m_logFile << str.toStdString();
    m_logFile.flush();
    m_curLines++;

    return true;
}

QString AutoLogger::generateSessionString(int stringLength)
{
    const QString possibleCharacters(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString randomString;

    for (int i = 0; i < stringLength; ++i) {
        int index = std::rand() % possibleCharacters.length();
        QChar nextChar = possibleCharacters.at(index);
        randomString.append(nextChar);
    }

    return randomString;
}

QString AutoLogger::generateTitle()
{
    return getConfig().autoLog.autoLogDirectory + "/Session_"
           + AutoLogger::generateSessionString(SESSION_STR_LENGTH) + "_"
           + QDate::currentDate().toString("ddMMyy");
}

void AutoLogger::writeToLog(const QByteArray &ba)
{
    writeLine(ba);
}

void AutoLogger::shouldLog(bool echo)
{
    m_shouldLog = echo;
}

void AutoLogger::onConnected()
{
    m_curFile++; // If we recieve a new connection, create a new log file.
    if (getConfig().autoLog.autoLog) {
        createFile();
    }
}
