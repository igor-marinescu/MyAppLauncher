# MyAppLauncher - Qt 5.1.1 / Win32-Win64
QT       += core gui widgets
TARGET    = MyAppLauncher
TEMPLATE  = app

win32:RC_FILE = src/myapplauncher.rc

SOURCES  += src/main.cpp \
            src/configparser.cpp \
            src/processutils.cpp \
            src/launcherwindow.cpp

HEADERS  += src/programentry.h \
            src/configparser.h \
            src/processutils.h \
            src/launcherwindow.h

win32 {
    DEFINES += NOMINMAX UNICODE _UNICODE
    LIBS    += -luser32 -lshell32 -lkernel32
}

OTHER_FILES += \
            src/myapplauncher.rc
