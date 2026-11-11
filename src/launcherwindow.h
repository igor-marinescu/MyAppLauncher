#ifndef LAUNCHERWINDOW_H
#define LAUNCHERWINDOW_H

#include <QWidget>
#include <QList>
#include "programentry.h"

class QLineEdit;
class QListView;
class QStandardItemModel;
class QModelIndex;

class LauncherWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LauncherWindow(const QList<ProgramEntry> &programs, QWidget *parent = 0);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void changeEvent(QEvent *event);
    void closeEvent(QCloseEvent *event);

private slots:
    void updateProgramList(const QString &text);
    void launchSelected();
    void onItemActivated(const QModelIndex &index);

private:
    void moveSelection(int delta);
    void sendToBackground();
    void focusInput();
    void restoreWindowGeometry();
    void saveWindowGeometry();

    QList<ProgramEntry>  m_programs;
    QLineEdit           *m_input;
    QListView           *m_list;
    QStandardItemModel  *m_model;
};

#endif // LAUNCHERWINDOW_H
