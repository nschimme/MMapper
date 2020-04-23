// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include "autologger.h"

#include "../configuration/configuration.h"
#include "../parser/parserutils.h"

AutoLogger::AutoLogger(QObject *const parent)
    : QObject(parent)
    , m_logPrefix{generateLogPrefix()}
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

    const auto &settings = getConfig().autoLog;

    auto dir = QDir(settings.autoLogDirectory);
    if (!dir.exists())
        dir.mkdir(".");

    QString fileName = QString("%2_%3.txt").arg(m_logPrefix).arg(QString::number(m_curFile));
    m_logFile.open(dir.absoluteFilePath(fileName).toStdString(),
                   std::fstream::out | std::fstream::app);
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

    } else if (m_curLines > getConfig().autoLog.autoLogMaxLines) {
        m_logFile.close();
        if (!createFile())
            return false;
    }

    QString str = QString::fromLatin1(ba);
    ParserUtils::removeAnsiMarksInPlace(str);

    m_logFile << str.toStdString();
    m_logFile.flush();
    m_curLines++;

    return true;
}

void AutoLogger::deleteOldLogs()
{
    auto &settings = getConfig();

    const QDate today = QDate::currentDate();
    QList<QFileInfo> files;

    Q_FOREACH (auto fileInfo,
               QDir(settings.autoLog.autoLogDirectory)
                   .entryInfoList(QStringList("MMapper_Log_*.txt"), QDir::Files)) {
        if (fileInfo.created().date().daysTo(today) >= settings.autoLog.deleteLogsOlderThan) {
            files.append(fileInfo);
        }
    }

    if (settings.autoLog.warnWhenDeleting && files.length() > settings.autoLog.warnWhenMoreThan) {
        QMessageBox msgBox;
        msgBox.setText("There are more than " + QString::number(settings.autoLog.warnWhenMoreThan)
                       + " logs to be deleted.");
        msgBox.setInformativeText("Continue?");
        msgBox.setWindowTitle("MMapper Warning");
        msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);

        int ret = msgBox.exec();
        if (ret == QMessageBox::No)
            return;
    }
    QFileInfoList fileList(files);
    deleteLogs(fileList);
}

void AutoLogger::deleteLogs(const QFileInfoList &files)
{
    Q_FOREACH (auto fileInfo, files) {
        QString filepath = fileInfo.absoluteFilePath();
        QDir deletefile;
        deletefile.setPath(filepath);
        deletefile.remove(filepath);
        qDebug() << "Deleted log " + filepath + ".";
    }
}

QString AutoLogger::generateLogPrefix()
{
    return QString("MMapper_Log_%1_%2")
        .arg(QDate::currentDate().toString("yyyy_MM_dd"))
        .arg(QString::number(QDateTime::currentDateTimeUtc().toTime_t()));
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
    if (getConfig().autoLog.deleteOldLogs)
        deleteOldLogs();

    if (getConfig().autoLog.autoLog)
        createFile();
}
