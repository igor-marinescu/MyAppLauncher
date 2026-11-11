#ifndef PROCESSUTILS_H
#define PROCESSUTILS_H

#include <QString>
#include <QStringList>
#include "programentry.h"

namespace ProcessUtils
{
    // Expands %VARIABLES% in a string (e.g. %ProgramFiles%).
    QString expandEnvironment(const QString &s);

    // Returns the absolute native path of an existing executable, or an empty
    // string. Relative names are searched in the application dir and PATH.
    QString resolveExecutable(const QString &exe);

    // Builds the command line: "<exe>" <argument1> <argument2> ...
    QString buildCommandLine(const ProgramEntry &entry);

    // Splits a command line into tokens using Windows rules (CommandLineToArgvW).
    QStringList splitCommandLine(const QString &commandLine);

    // Starts the program. Returns false and sets 'error' on failure.
    bool startProgram(const ProgramEntry &entry, QString *error);

    // Looks for a running process matching the entry (exe, and also the
    // arguments if 'matchArguments' is true). If one with a top-level window is
    // found, the window is activated and true is returned.
    bool switchToRunning(const ProgramEntry &entry, bool matchArguments);

    // Executes the entry according to its option (start or switch).
    bool launch(const ProgramEntry &entry, QString *error);
}

#endif // PROCESSUTILS_H
