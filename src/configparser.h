#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <QList>
#include <QMap>
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

    // Variables defined with "set <name> = <value>". Keys are lower case
    // (variable names are case-insensitive).
    typedef QMap<QString, QString> Variables;

    // If 'line' is a variable definition ("set <name> = <value>"), returns true
    // and sets 'isValid'. When valid, the value (with variables already
    // expanded) is stored in 'vars'; otherwise 'reason' is set.
    static bool parseVariableDefinition(const QString &line, Variables &vars,
                                        bool &isValid, QString &reason);

    // Replaces every "$name" in 'text' with the variable's value; "$$" gives a
    // literal '$'. A '$' not followed by a name character is kept as is.
    // Returns false and sets 'reason' if an undefined variable is used.
    static bool expandVariables(const QString &text, const Variables &vars,
                                QString &result, QString &reason);
};

#endif // CONFIGPARSER_H
