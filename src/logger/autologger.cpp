#include "autologger.h"

AutoLogger::AutoLogger(int maxLogLines)
{
    maxLines = maxLogLines;
    curFile = 0;
    title = (QDate::currentDate().toString("ddMMyy") + "_"
               + QTime::currentTime().toString("hhmmss"));

    createFile();
}

AutoLogger::~AutoLogger(){
    AutoLogger::logFile.flush();
    AutoLogger::logFile.close();
}

bool AutoLogger::createFile()
{
    QString fileName = QString(title+QString::number(curFile)+".txt");
    AutoLogger::logFile.open(fileName.toStdString(), std::ofstream::out);

    if (!AutoLogger::logOpen())
    {
        qDebug() << "Could not create file.";
        return false;

    }
    curFile++;
    curLines = 0;
    return true;
}

void AutoLogger::writeLine(QByteArray &line){
    if (!AutoLogger::logOpen())
    {
        qDebug("Tried to write to a closed log.");
        return;

    }

    if (curLines > maxLines)
        if (!createFile())
            return;

    AutoLogger::logFile << ((QString)line).toStdString();
    curLines++;

}
