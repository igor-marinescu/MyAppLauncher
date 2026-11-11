#include "configparser.h"
#include "processutils.h"

#include <QFile>
#include <QTextStream>

bool ConfigParser::load(const QString &path,
                        QList<ProgramEntry> &entries,
                        QList<LineError> &errors,
                        QString *fileError)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (fileError)
            *fileError = file.errorString();
        return false;
    }

    QTextStream in(&file);   // auto-detects UTF-8/UTF-16 BOM, otherwise local 8-bit
    int lineNumber = 0;
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        ++lineNumber;

        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        ProgramEntry entry;
        QString reason;
        if (parseLine(line, entry, reason)) {
            entry.lineNumber = lineNumber;
            entries.append(entry);
        } else {
            LineError err;
            err.lineNumber = lineNumber;
            err.reason = reason;
            errors.append(err);
        }
    }
    return true;
}

bool ConfigParser::parseLine(const QString &line, ProgramEntry &entry, QString &reason)
{
    const QStringList fields = line.split(QLatin1Char(';'));
    if (fields.size() < 3) {
        reason = QString::fromLatin1("Expected at least 3 fields: <name>;<options>;<exe>[;<arguments>...]");
        return false;
    }

    // name
    entry.name = fields.at(0).trimmed();
    if (entry.name.isEmpty()) {
        reason = QString::fromLatin1("Program name is empty.");
        return false;
    }

    // options
    const QString opt = fields.at(1).trimmed().toLower();
    if (opt.isEmpty())
        entry.option = ProgramEntry::OptionNone;
    else if (opt == QLatin1String("once_exe"))
        entry.option = ProgramEntry::OptionOnceExe;
    else if (opt == QLatin1String("once_exe_arg"))
        entry.option = ProgramEntry::OptionOnceExeArg;
    else {
        reason = QString::fromLatin1("Unknown option \"%1\" (allowed: empty, once_exe, once_exe_arg).")
                     .arg(fields.at(1).trimmed());
        return false;
    }

    // exe
    QString exe = fields.at(2).trimmed();
    if (exe.size() >= 2 && exe.startsWith(QLatin1Char('"')) && exe.endsWith(QLatin1Char('"')))
        exe = exe.mid(1, exe.size() - 2).trimmed();
    if (exe.isEmpty()) {
        reason = QString::fromLatin1("Exe field is empty.");
        return false;
    }
    entry.exe = ProcessUtils::expandEnvironment(exe);
    entry.exeResolved = ProcessUtils::resolveExecutable(entry.exe);
    if (entry.exeResolved.isEmpty()) {
        reason = QString::fromLatin1("Exe file \"%1\" not found.").arg(entry.exe);
        return false;
    }

    // arguments (empty fields, e.g. caused by a trailing ';', are skipped)
    entry.arguments.clear();
    for (int i = 3; i < fields.size(); ++i) {
        const QString arg = fields.at(i).trimmed();
        if (!arg.isEmpty())
            entry.arguments.append(arg);
    }
    return true;
}
