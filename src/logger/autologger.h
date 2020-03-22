#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include <fstream>
#include <QObject>

static constexpr const int AWAIT_MUMESOCKET_CONNECTION = -1;

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

    bool createFile();
    QString generateSessionString(int length);
    QString generateTitle();

private:
    QString m_sessionString;
    QString m_title;
    std::fstream m_logFile;
    int m_curLines = AWAIT_MUMESOCKET_CONNECTION;
    int m_curFile = 0;
    bool m_shouldLog = true;
};
