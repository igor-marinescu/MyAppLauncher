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
    Variables vars;
    int lineNumber = 0;
    while (!in.atEnd()) {
        const QString raw = in.readLine();
        ++lineNumber;

        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        QString reason;

        // Variable definition: "set <name> = <value>"
        bool varValid = false;
        if (parseVariableDefinition(line, vars, varValid, reason)) {
            if (!varValid) {
                LineError err;
                err.lineNumber = lineNumber;
                err.reason = reason;
                errors.append(err);
            }
            continue;
        }

        // Program definition: replace the variables first, then parse.
        QString expanded;
        ProgramEntry entry;
        if (expandVariables(line, vars, expanded, reason)
                && parseLine(expanded, entry, reason)) {
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

static bool isVarNameStart(QChar c)
{
    return c == QLatin1Char('_')
        || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
        || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
}

static bool isVarNameChar(QChar c)
{
    return isVarNameStart(c) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
}

bool ConfigParser::parseVariableDefinition(const QString &line, Variables &vars,
                                           bool &isValid, QString &reason)
{
    // A definition starts with the keyword "set" followed by white space and
    // contains '=' before any ';' (so a program line is never mistaken for one).
    if (line.size() < 4
            || !line.startsWith(QLatin1String("set"), Qt::CaseInsensitive)
            || !line.at(3).isSpace())
        return false;
    const int eq = line.indexOf(QLatin1Char('='));
    const int semi = line.indexOf(QLatin1Char(';'));
    if (eq < 0 || (semi >= 0 && semi < eq))
        return false;

    isValid = false;
    const QString name = line.mid(4, eq - 4).trimmed();
    bool nameOk = !name.isEmpty() && isVarNameStart(name.at(0));
    for (int i = 1; nameOk && i < name.size(); ++i)
        nameOk = isVarNameChar(name.at(i));
    if (!nameOk) {
        reason = QString::fromLatin1("Invalid variable name \"%1\" (allowed: letters, digits and '_', "
                                     "not starting with a digit).").arg(name);
        return true;
    }

    // The value may itself use variables defined earlier.
    QString value;
    if (!expandVariables(line.mid(eq + 1).trimmed(), vars, value, reason))
        return true;

    vars.insert(name.toLower(), value);
    isValid = true;
    return true;
}

bool ConfigParser::expandVariables(const QString &text, const Variables &vars,
                                   QString &result, QString &reason)
{
    result.clear();
    result.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        const QChar c = text.at(i);
        if (c != QLatin1Char('$')) {
            result.append(c);
            ++i;
            continue;
        }
        // "$$" -> literal '$'
        if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('$')) {
            result.append(QLatin1Char('$'));
            i += 2;
            continue;
        }
        // "$name" -> the longest run of name characters
        int end = i + 1;
        if (end < text.size() && isVarNameStart(text.at(end))) {
            while (end < text.size() && isVarNameChar(text.at(end)))
                ++end;
        }
        if (end == i + 1) {             // '$' not followed by a name: keep it
            result.append(c);
            ++i;
            continue;
        }
        const QString name = text.mid(i + 1, end - i - 1);
        const Variables::const_iterator it = vars.constFind(name.toLower());
        if (it == vars.constEnd()) {
            reason = QString::fromLatin1("Undefined variable \"$%1\" (define it before use with "
                                         "\"set %1 = <value>\"; write \"$$\" for a literal '$').")
                         .arg(name);
            return false;
        }
        result.append(it.value());
        i = end;
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
