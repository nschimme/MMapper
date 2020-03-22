#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include <fstream>
#include <QObject>

class AutoLogger : public QObject
{
    Q_OBJECT
public:
    explicit AutoLogger(QObject *parent);
    ~AutoLogger() override;

    void close();

public slots:
    void writeToLog(const QByteArray &ba);
    void shouldLog(bool echo);
    void onConnected();

private:
    bool writeLine(const QByteArray &ba);

    bool startedToPlay(const QByteArray &data);
    bool createFile();
    QString generateSessionString(int length);
    QString generateTitle();

private:
    const int sessionStrLength = 5; // Random session string length.
    int m_maxLines;
    int m_curLines;
    int m_curFile;

    bool m_overwriteOld;
    bool m_shouldLog;

    std::fstream m_logFile;

    QString m_sessionString;
    QString m_title;
};
