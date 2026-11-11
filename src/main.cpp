#include <QApplication>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMessageBox>
#include <QStringList>

#include "configparser.h"
#include "launcherwindow.h"

static const char *kConfigFileName = "config.txt";

// config.txt is searched next to the executable, then in the current directory.
// An explicit path can be passed as the first command-line argument.
static QString findConfigFile()
{
    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1)
        return args.at(1);

    const QString appDirConfig = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QString::fromLatin1(kConfigFileName));
    if (QFileInfo(appDirConfig).exists())
        return appDirConfig;

    const QString cwdConfig = QDir::current().filePath(QString::fromLatin1(kConfigFileName));
    if (QFileInfo(cwdConfig).exists())
        return cwdConfig;

    return appDirConfig;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QString::fromLatin1("MyAppLauncher"));

    const QString configPath = findConfigFile();
    const QString configName = QFileInfo(configPath).fileName();

    QList<ProgramEntry> programs;
    QList<ConfigParser::LineError> errors;
    QString fileError;

    if (!ConfigParser::load(configPath, programs, errors, &fileError)) {
        QMessageBox::warning(0, app.applicationName(),
                             QString::fromLatin1("Cannot open %1:\n%2")
                                 .arg(QDir::toNativeSeparators(configPath), fileError));
    }

    foreach (const ConfigParser::LineError &err, errors) {
        QMessageBox box(QMessageBox::Warning, app.applicationName(),
                        QString::fromLatin1("%1 line # %2 invalid, ignored")
                            .arg(configName).arg(err.lineNumber),
                        QMessageBox::Ok);
        box.setInformativeText(err.reason);
        box.exec();
    }

    // Extract the program icons from the exe files.
    QFileIconProvider iconProvider;
    for (int i = 0; i < programs.size(); ++i)
        programs[i].icon = iconProvider.icon(QFileInfo(programs[i].exeResolved));

    LauncherWindow window(programs);
    window.show();
    window.raise();
    window.activateWindow();

    return app.exec();
}
