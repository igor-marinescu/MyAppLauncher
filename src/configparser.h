#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <QList>
#include <QString>
#include "programentry.h"

class ConfigParser
{
public:
    struct LineError
    {
        int     lineNumber;
        QString reason;
    };

    // Parses the configuration file. Valid lines are appended to 'entries',
    // invalid lines to 'errors'. Returns false if the file cannot be opened.
    static bool load(const QString &path,
                     QList<ProgramEntry> &entries,
                     QList<LineError> &errors,
                     QString *fileError = 0);

    // Parses a single (non-empty, non-comment) line. Returns false and sets
    // 'reason' if the line is invalid.
    static bool parseLine(const QString &line, ProgramEntry &entry, QString &reason);
};

#endif // CONFIGPARSER_H
