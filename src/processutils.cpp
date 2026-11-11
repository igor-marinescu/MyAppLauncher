#include "processutils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QVector>

#include <string>
#include <vector>

#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

namespace
{
// ---------------------------------------------------------------------------
// Dynamically loaded APIs (not declared in all SDK headers)
// ---------------------------------------------------------------------------
typedef BOOL (WINAPI *QueryFullProcessImageNameW_t)(HANDLE, DWORD, LPWSTR, PDWORD);
typedef BOOL (WINAPI *IsWow64Process_t)(HANDLE, PBOOL);
typedef LONG (NTAPI  *NtQueryInformationProcess_t)(HANDLE, int, PVOID, ULONG, PULONG);

struct NtUnicodeString
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
};

const int  kProcessCommandLineInformation = 60;          // Windows 8.1+
const LONG kStatusInfoLengthMismatch      = (LONG)0xC0000004L;
const LONG kStatusBufferOverflow          = (LONG)0x80000005L;
const LONG kStatusBufferTooSmall          = (LONG)0xC0000023L;

QueryFullProcessImageNameW_t queryFullProcessImageName()
{
    static QueryFullProcessImageNameW_t fn = (QueryFullProcessImageNameW_t)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "QueryFullProcessImageNameW");
    return fn;
}

NtQueryInformationProcess_t ntQueryInformationProcess()
{
    static NtQueryInformationProcess_t fn = (NtQueryInformationProcess_t)
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    return fn;
}

bool isWow64()
{
    static int cached = -1;
    if (cached < 0) {
        cached = 0;
        IsWow64Process_t fn = (IsWow64Process_t)
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process");
        BOOL wow = FALSE;
        if (fn && fn(GetCurrentProcess(), &wow) && wow)
            cached = 1;
    }
    return cached == 1;
}

QString windowsDir()
{
    wchar_t winDir[MAX_PATH];
    UINT n = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return QString();
    return QString::fromWCharArray(winDir, (int)n);
}

// A 32-bit process on 64-bit Windows is redirected from System32 to SysWOW64.
// Map "%windir%\System32\..." to "%windir%\Sysnative\..." so the native
// (64-bit) program is checked and started, as the user expects.
QString mapSystem32ToSysnative(const QString &nativePath)
{
    if (!isWow64())
        return nativePath;
    const QString win = windowsDir();
    if (win.isEmpty())
        return nativePath;
    const QString sys32 = win + QLatin1String("\\System32\\");
    if (nativePath.startsWith(sys32, Qt::CaseInsensitive))
        return win + QLatin1String("\\Sysnative\\") + nativePath.mid(sys32.size());
    return nativePath;
}

// Reverse mapping: running processes always report System32, never Sysnative.
QString mapSysnativeToSystem32(const QString &nativePath)
{
    if (!isWow64())
        return nativePath;
    const QString win = windowsDir();
    if (win.isEmpty())
        return nativePath;
    const QString sysnative = win + QLatin1String("\\Sysnative\\");
    if (nativePath.startsWith(sysnative, Qt::CaseInsensitive))
        return win + QLatin1String("\\System32\\") + nativePath.mid(sysnative.size());
    return nativePath;
}

QString normalizePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
}

QString processImagePath(HANDLE process)
{
    QueryFullProcessImageNameW_t fn = queryFullProcessImageName();
    if (!fn)
        return QString();
    QVector<wchar_t> buf(32768);
    DWORD size = (DWORD)buf.size();
    if (!fn(process, 0, buf.data(), &size))
        return QString();
    return QString::fromWCharArray(buf.constData(), (int)size);
}

QString processCommandLine(HANDLE process)
{
    NtQueryInformationProcess_t fn = ntQueryInformationProcess();
    if (!fn)
        return QString();

    ULONG needed = 0;
    QVector<char> buf(4096);
    LONG status = fn(process, kProcessCommandLineInformation, buf.data(), (ULONG)buf.size(), &needed);
    if ((status == kStatusInfoLengthMismatch || status == kStatusBufferOverflow
         || status == kStatusBufferTooSmall) && needed > (ULONG)buf.size()) {
        buf.resize((int)needed);
        status = fn(process, kProcessCommandLineInformation, buf.data(), (ULONG)buf.size(), &needed);
    }
    if (status < 0)
        return QString();

    const NtUnicodeString *us = reinterpret_cast<const NtUnicodeString *>(buf.constData());
    if (!us->Buffer || us->Length == 0)
        return QString();
    return QString::fromWCharArray(us->Buffer, us->Length / (int)sizeof(wchar_t));
}

// True if all 'wanted' tokens appear in 'actual' in the same order
// (case-insensitive). Allows extra arguments added by the running program.
bool containsArguments(const QStringList &actual, const QStringList &wanted)
{
    int pos = 0;
    foreach (const QString &w, wanted) {
        bool found = false;
        while (pos < actual.size()) {
            if (actual.at(pos++).compare(w, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Top-level window lookup / activation
// ---------------------------------------------------------------------------
struct WindowSearch
{
    DWORD pid;
    HWND  found;
};

BOOL CALLBACK findWindowProc(HWND hwnd, LPARAM lParam)
{
    WindowSearch *s = reinterpret_cast<WindowSearch *>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != s->pid)
        return TRUE;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != NULL)
        return TRUE;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return TRUE;
    s->found = hwnd;
    return FALSE;   // stop enumeration (Z-order: topmost window of the process)
}

HWND findMainWindow(DWORD pid)
{
    WindowSearch s;
    s.pid = pid;
    s.found = NULL;
    EnumWindows(findWindowProc, reinterpret_cast<LPARAM>(&s));
    return s.found;
}

void activateWindow(HWND hwnd)
{
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else
        ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
}

QString quoteIfNeeded(const QString &s)
{
    if (s.contains(QLatin1Char(' ')) || s.contains(QLatin1Char('\t')))
        return QLatin1Char('"') + s + QLatin1Char('"');
    return s;
}

} // namespace

// ===========================================================================

QString ProcessUtils::expandEnvironment(const QString &s)
{
    if (!s.contains(QLatin1Char('%')))
        return s;
    const std::wstring in = s.toStdWString();
    DWORD n = ExpandEnvironmentStringsW(in.c_str(), NULL, 0);
    if (n == 0)
        return s;
    QVector<wchar_t> buf((int)n + 1);
    n = ExpandEnvironmentStringsW(in.c_str(), buf.data(), (DWORD)buf.size());
    if (n == 0 || n > (DWORD)buf.size())
        return s;
    return QString::fromWCharArray(buf.constData());
}

QString ProcessUtils::resolveExecutable(const QString &exe)
{
    const QString native = QDir::toNativeSeparators(exe);
    QFileInfo fi(native);

    if (fi.isAbsolute()) {
        const QString checkPath = mapSystem32ToSysnative(QDir::toNativeSeparators(fi.absoluteFilePath()));
        QFileInfo check(checkPath);
        if (check.exists() && check.isFile())
            return checkPath;
        return QString();
    }

    // Relative: application directory first, then the standard search path.
    QFileInfo appRel(QDir(QCoreApplication::applicationDirPath()), native);
    if (appRel.exists() && appRel.isFile())
        return QDir::toNativeSeparators(appRel.absoluteFilePath());

    const std::wstring name = native.toStdWString();
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(NULL, name.c_str(), L".exe", MAX_PATH, buf, NULL);
    if (n > 0 && n < MAX_PATH)
        return mapSystem32ToSysnative(QString::fromWCharArray(buf, (int)n));
    return QString();
}

QString ProcessUtils::buildCommandLine(const ProgramEntry &entry)
{
    QString cmd = quoteIfNeeded(QDir::toNativeSeparators(entry.exe));
    foreach (const QString &arg, entry.arguments)
        cmd += QLatin1Char(' ') + arg;
    return cmd;
}

QStringList ProcessUtils::splitCommandLine(const QString &commandLine)
{
    QStringList result;
    if (commandLine.trimmed().isEmpty())
        return result;
    const std::wstring cl = commandLine.toStdWString();
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(cl.c_str(), &argc);
    if (!argv)
        return result;
    for (int i = 0; i < argc; ++i)
        result.append(QString::fromWCharArray(argv[i]));
    LocalFree(argv);
    return result;
}

bool ProcessUtils::startProgram(const ProgramEntry &entry, QString *error)
{
    const std::wstring app = entry.exeResolved.toStdWString();
    const QString cmd = buildCommandLine(entry);
    const std::wstring cmdStr = cmd.toStdWString();
    std::vector<wchar_t> cmdBuf(cmdStr.begin(), cmdStr.end());
    cmdBuf.push_back(L'\0');   // CreateProcessW requires a writable buffer

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(app.c_str(), &cmdBuf[0], NULL, NULL, FALSE, 0,
                        NULL, NULL, &si, &pi)) {
        const DWORD err = GetLastError();
        if (error)
            *error = QString::fromLatin1("Cannot start \"%1\":\n%2").arg(cmd, qt_error_string((int)err));
        return false;
    }
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool ProcessUtils::switchToRunning(const ProgramEntry &entry, bool matchArguments)
{
    const QString wantedExe  = mapSysnativeToSystem32(entry.exeResolved);
    const QString wantedPath = normalizePath(wantedExe);
    const QString wantedName = QFileInfo(wantedExe).fileName();

    QStringList wantedArgs;
    if (matchArguments && !entry.arguments.isEmpty()) {
        // Tokenize the configured arguments with the same rules as the running one.
        wantedArgs = splitCommandLine(QLatin1String("x ") + entry.arguments.join(QLatin1String(" ")));
        if (!wantedArgs.isEmpty())
            wantedArgs.removeFirst();
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    const DWORD selfPid = GetCurrentProcessId();
    bool switched = false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !switched; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == 0 || pe.th32ProcessID == selfPid)
            continue;

        // Quick filter on the exe file name before opening the process.
        if (QString::fromWCharArray(pe.szExeFile).compare(wantedName, Qt::CaseInsensitive) != 0)
            continue;

        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h)
            continue;

        bool match = false;
        const QString image = processImagePath(h);
        // Same exe: full path compared when available, else file name only.
        if (image.isEmpty() || normalizePath(image) == wantedPath) {
            match = true;
            if (matchArguments) {
                QStringList actualArgs = splitCommandLine(processCommandLine(h));
                if (!actualArgs.isEmpty())
                    actualArgs.removeFirst();   // drop the exe
                match = containsArguments(actualArgs, wantedArgs);
            }
        }
        CloseHandle(h);

        if (match) {
            HWND hwnd = findMainWindow(pe.th32ProcessID);
            if (hwnd) {
                activateWindow(hwnd);
                switched = true;
            }
        }
    }
    CloseHandle(snap);
    return switched;
}

bool ProcessUtils::launch(const ProgramEntry &entry, QString *error)
{
    switch (entry.option) {
    case ProgramEntry::OptionOnceExe:
        if (switchToRunning(entry, false))
            return true;
        break;
    case ProgramEntry::OptionOnceExeArg:
        if (switchToRunning(entry, true))
            return true;
        break;
    case ProgramEntry::OptionNone:
    default:
        break;
    }
    return startProgram(entry, error);
}
