#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include <fstream>
#include <QObject>
#include <QFileInfo>
#include <QMessageBox>

class AutoLogger : public QObject
{
    Q_OBJECT
public:
    explicit AutoLogger(QObject *parent);
    ~AutoLogger() override;

public slots:
    void writeToLog(const QByteArray &ba);
    void shouldLog(bool echo);
    void onConnected();

private:
    bool writeLine(const QByteArray &ba);
    void deleteOldLogs();
    void deleteLogs(const QFileInfoList &files);

    bool createFile();
    QString generateLogPrefix();

private:
    QString m_logPrefix;
    std::fstream m_logFile;
    int m_curLines = 0;
    int m_curFile = 0;
    bool m_shouldLog = true;
};
