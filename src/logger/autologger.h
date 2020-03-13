#pragma once

#include <QtCore>

#include "../configuration/configuration.h"
#include "../parser/parserutils.h"
#include "../global/utils.h"

#include <fstream>

class AutoLogger : public QObject {
    Q_OBJECT
public:
    explicit AutoLogger();
    ~AutoLogger() override;

    void writeLine(const QByteArray &ba);
    void close();

public slots:
    void onUserInput(const QByteArray &ba);

private:
    bool startedToPlay(const QByteArray &data);
    bool createFile();
    QString getTitle(){
        return getConfig().autoLog.autoLogDirectory + "/Log_" +
                    (QDate::currentDate().toString("ddMMyy"));
    }

    bool m_shouldLog = false;
    std::fstream m_logFile;
    int m_curFile = 0;
    QString m_title;
    int m_maxLines;
    int m_curLines;
};
