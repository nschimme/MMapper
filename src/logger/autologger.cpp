// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include "autologger.h"

AutoLogger::AutoLogger()
{
    m_maxLines = getConfig().autoLog.autoLogMaxLines;
    createFile();
}

AutoLogger::~AutoLogger()
{
    m_logFile.flush();
    m_logFile.close();
}

bool AutoLogger::createFile()
{
    m_title = getTitle();
    QString fileName = QString(m_title + "_" + QString::number(m_curFile) + ".txt");

    m_logFile.open(fileName.toStdString(), std::fstream::out | std::fstream::app);
    if (!m_logFile.is_open()) {
        qDebug() << "Could not create file.";
        return false;
    }

    m_curFile++;
    m_curLines = 0;
    return true;
}

void AutoLogger::writeLine(const QByteArray &line)
{
    if (!m_shouldLog)
        return;

    if (!m_logFile.is_open()) {
        qDebug("Tried to write to a closed log.");
        return;
    }

    QString str = QString::fromLatin1(line);
    ;
    if (str.contains('\x1b'))
        ParserUtils::removeAnsiMarksInPlace(str);

    if (m_curLines > m_maxLines) {
        m_logFile.close();
        if (!createFile())
            return;
    }

    m_logFile << str.toStdString();
    m_logFile.flush();

    m_curLines++;
}

void AutoLogger::onUserInput(const QByteArray &ba)
{
    if (ba.contains("play ")) // We don't want to log user login information.
        m_shouldLog = true;

    writeLine(ba);
}
