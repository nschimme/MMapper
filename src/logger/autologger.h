#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias 'Mew_' Viklund <devmew@exedump.com> (Mirnir)

#include "../global/macros.h"

#include <fstream>
#include <string>

#include <QFileInfoList>
#include <QObject>

class AutoLogger final : public QObject
{
    Q_OBJECT
public:
    explicit AutoLogger(QObject *parent);
    ~AutoLogger() final;

public slots:
    void slot_writeToLog(const QString &str);
    void slot_shouldLog(bool echo);
    void slot_onConnected();

private:
    NODISCARD bool writeLine(const QString &str);
    void deleteOldLogs();
    void deleteLogs(const QFileInfoList &files);
    NODISCARD bool showDeleteDialog(QString message);
    NODISCARD bool createFile();

private:
    const std::string m_runId;
    std::fstream m_logFile;
    int m_curBytes = 0;
    int m_curFile = 0;
    bool m_shouldLog = true;
};
