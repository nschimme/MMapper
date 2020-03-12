#pragma once

#include <QtCore>
#include "../global/utils.h"
#include <fstream>
class AutoLogger{
public:
    AutoLogger(int maxlines);
    ~AutoLogger();
    static bool logOpen(){
        return AutoLogger::logFile.is_open();
    }

    static void writeLine(QByteArray &line);

private:
    static bool createFile();

    static std::ofstream logFile;
    static int curLines;

    static QString title;
    static int maxLines;
    static int curFile;
};
