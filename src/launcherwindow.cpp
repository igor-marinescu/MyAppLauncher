#include "launcherwindow.h"
#include "processutils.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QSettings>
#include <QStandardItemModel>
#include <QVBoxLayout>

static const int kProgramIndexRole = Qt::UserRole + 1;
static const char *kGeometryKey    = "window/geometry";

// Window position/size are stored in MyAppLauncher.ini next to the executable.
static QString settingsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QString::fromLatin1("MyAppLauncher.ini"));
}

static QString optionText(ProgramEntry::Option option)
{
    switch (option) {
    case ProgramEntry::OptionOnceExe:    return QString::fromLatin1("once_exe");
    case ProgramEntry::OptionOnceExeArg: return QString::fromLatin1("once_exe_arg");
    case ProgramEntry::OptionNone:
    default:                             return QString();
    }
}

// <name> (<options> <exe_file_without_path> <arguments_string>)
// Empty options / arguments are omitted.
static QString displayText(const ProgramEntry &p)
{
    QStringList details;
    const QString opt = optionText(p.option);
    if (!opt.isEmpty())
        details.append(opt);
    details.append(QFileInfo(QDir::fromNativeSeparators(p.exe)).fileName());
    if (!p.arguments.isEmpty())
        details.append(p.arguments.join(QLatin1String(" ")));
    return p.name + QLatin1String(" (") + details.join(QLatin1String(" ")) + QLatin1Char(')');
}

LauncherWindow::LauncherWindow(const QList<ProgramEntry> &programs, QWidget *parent)
    : QWidget(parent),
      m_programs(programs),
      m_input(new QLineEdit(this)),
      m_list(new QListView(this)),
      m_model(new QStandardItemModel(this))
{
    setWindowTitle(QString::fromLatin1("MyAppLauncher"));
    resize(520, 320);   // default size, used if no geometry was saved
    restoreWindowGeometry();

    m_input->setPlaceholderText(QString::fromLatin1("Type program name..."));

    m_list->setModel(m_model);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(24, 24));
    m_list->setUniformItemSizes(true);
    m_list->setFocusPolicy(Qt::NoFocus);   // keyboard focus always stays in the input

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(m_input);
    layout->addWidget(m_list, 1);

    m_input->installEventFilter(this);

    connect(m_input, SIGNAL(textChanged(QString)), this, SLOT(updateProgramList(QString)));
    connect(m_input, SIGNAL(returnPressed()), this, SLOT(launchSelected()));
    connect(m_list, SIGNAL(activated(QModelIndex)), this, SLOT(onItemActivated(QModelIndex)));

    updateProgramList(QString());
    m_input->setFocus();
}

// Rebuilds the Program List: exact matches first, then prefix matches,
// each group in configuration-file order. The first entry is selected.
void LauncherWindow::updateProgramList(const QString &text)
{
    QList<int> exact;
    QList<int> prefix;
    for (int i = 0; i < m_programs.size(); ++i) {
        const QString &name = m_programs.at(i).name;
        if (text.isEmpty())
            prefix.append(i);
        else if (name.compare(text, Qt::CaseInsensitive) == 0)
            exact.append(i);
        else if (name.startsWith(text, Qt::CaseInsensitive))
            prefix.append(i);
    }

    m_model->clear();
    const QList<int> ordered = exact + prefix;
    foreach (int i, ordered) {
        const ProgramEntry &p = m_programs.at(i);
        QStandardItem *item = new QStandardItem(p.icon, displayText(p));
        item->setData(i, kProgramIndexRole);
        item->setToolTip(ProcessUtils::buildCommandLine(p));
        m_model->appendRow(item);
    }

    if (m_model->rowCount() > 0) {
        const QModelIndex first = m_model->index(0, 0);
        m_list->setCurrentIndex(first);
        m_list->scrollTo(first);
    }
}

void LauncherWindow::moveSelection(int delta)
{
    const int rows = m_model->rowCount();
    if (rows == 0)
        return;
    int row = m_list->currentIndex().isValid() ? m_list->currentIndex().row() : 0;
    row = qBound(0, row + delta, rows - 1);
    const QModelIndex idx = m_model->index(row, 0);
    m_list->setCurrentIndex(idx);
    m_list->scrollTo(idx);
}

void LauncherWindow::launchSelected()
{
    if (m_model->rowCount() == 0)
        return;
    QModelIndex idx = m_list->currentIndex();
    if (!idx.isValid())
        idx = m_model->index(0, 0);

    const int programIndex = idx.data(kProgramIndexRole).toInt();
    if (programIndex < 0 || programIndex >= m_programs.size())
        return;

    QString error;
    if (ProcessUtils::launch(m_programs.at(programIndex), &error))
        sendToBackground();   // started or switched to
    else
        QMessageBox::warning(this, windowTitle(), error);
}

void LauncherWindow::onItemActivated(const QModelIndex &index)
{
    m_list->setCurrentIndex(index);
    launchSelected();
}

void LauncherWindow::sendToBackground()
{
    showMinimized();
}

void LauncherWindow::restoreWindowGeometry()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QByteArray geometry = settings.value(QString::fromLatin1(kGeometryKey)).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);   // Qt moves the window back on screen if needed
}

void LauncherWindow::saveWindowGeometry()
{
    // saveGeometry() stores the normal (non-minimized) position and size,
    // so closing the window while it is minimized works as well.
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kGeometryKey), saveGeometry());
}

void LauncherWindow::closeEvent(QCloseEvent *event)
{
    saveWindowGeometry();
    QWidget::closeEvent(event);
}

void LauncherWindow::focusInput()
{
    m_input->setFocus();
    m_input->selectAll();
}

bool LauncherWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
            moveSelection(+1);
            return true;
        case Qt::Key_Up:
            moveSelection(-1);
            return true;
        case Qt::Key_Escape:
            sendToBackground();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LauncherWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        sendToBackground();
        return;
    }
    QWidget::keyPressEvent(event);
}

void LauncherWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    // When the window is restored (e.g. from the taskbar), the user can type again immediately.
    if ((event->type() == QEvent::WindowStateChange && !(windowState() & Qt::WindowMinimized))
        || (event->type() == QEvent::ActivationChange && isActiveWindow())) {
        focusInput();
    }
}
