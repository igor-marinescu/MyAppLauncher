#ifndef PROGRAMENTRY_H
#define PROGRAMENTRY_H

#include <QString>
#include <QStringList>
#include <QIcon>

// One program definition loaded from config.txt:
//   <name>;<options>;<exe>;<argument1>;<argument2>;...
struct ProgramEntry
{
    enum Option
    {
        OptionNone,         // always start the program
        OptionOnceExe,      // switch to a running instance with the same exe
        OptionOnceExeArg    // switch to a running instance with the same exe and arguments
    };

    ProgramEntry() : option(OptionNone), lineNumber(0) {}

    QString     name;        // user-defined name
    Option      option;      // start/switch option
    QString     exe;         // exe path as written in config.txt (env. variables expanded)
    QString     exeResolved; // absolute path used to start the program
    QStringList arguments;   // argument fields, used verbatim
    QIcon       icon;        // icon extracted from the exe file
    int         lineNumber;  // line in config.txt (1-based)
};

#endif // PROGRAMENTRY_H
