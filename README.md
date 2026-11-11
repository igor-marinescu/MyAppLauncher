# MyAppLauncher

A small keyboard-driven program launcher for Windows, written in C++ with Qt 5.1.1.

## What the app does

You list the programs you use regularly in a text file (`config.txt`), together with any command-line arguments they need. In MyAppLauncher you type the first few characters of a program's name and press **Enter**, and the program starts with its arguments.

For each program you can also choose to **switch to it if it's already running** instead of starting a second copy:

- `once_exe`: switch to a running process that has the same executable.
- `once_exe_arg`: switch to a running process that has the same executable **and** the same arguments, for example the same document already open in an editor.

Each entry in the list shows the program's icon, taken from its exe file. After a program has been started or switched to, the launcher minimizes itself. It remembers its window position and size between sessions.

## How to use it

### 1. Write `config.txt`

Each line defines one program:

```
<name>;<options>;<exe>;<argument1>;<argument2>;...
```

| Field | Meaning |
|---|---|
| `name` | The name you type in the launcher |
| `options` | Empty, `once_exe` or `once_exe_arg` |
| `exe` | Path to the executable. `%VARIABLES%` are expanded. A bare name such as `notepad.exe` is looked up in the application folder, then on `PATH` |
| `argumentX` | Arguments, passed exactly as written. Quotes are kept |

Empty lines and lines starting with `#` are ignored.

Example:

```
# name;options;exe;arguments...
putty localhost;;c:\Program Files\PuTTY\putty.exe;-load "ubuntu32gb";-l "igor"
calc;once_exe;C:\Windows\System32\calc.exe
excel hello;once_exe_arg;C:\Program Files (x86)\Microsoft Office\root\Office16\EXCEL.EXE;C:\hello.xls
notepad hosts;once_exe_arg;%windir%\System32\notepad.exe;C:\Windows\System32\drivers\etc\hosts
```

The command line is built as `"<exe>" <argument1> <argument2> ...`. The exe is quoted only if its path contains spaces.

#### Variables

To avoid repeating the same path or name many times, define a variable and use it as `$name`:

```
set project_path = C:\project\path
set editor = C:\Program Files\Notepad++\notepad++.exe

open_file;;notepad.exe;$project_path\file.txt
project notes;once_exe_arg;$editor;$project_path\notes.txt
```

`open_file` becomes `open_file;;notepad.exe;C:\project\path\file.txt`.

- `set <name> = <value>` defines a variable for all **following** lines. Defining it again changes the value from that line on.
- Names use letters, digits and `_` (not starting with a digit) and ignore upper/lower case.
- The value is the text after `=`, trimmed. It may use earlier variables and may contain `;`, so one variable can hold several arguments (`set args = -l "igor";-P 22`).
- `$name` takes the longest run of letters, digits and `_` after `$`. Write `$$` for a literal `$`. A `$` followed by anything else (e.g. `\\server\c$\dir`) is left as is.
- Variables are replaced in every field before the line is parsed; `%ENV%` variables in the exe are expanded afterwards.

**Predefined variable `config_file`:** holds the absolute path of the config file currently in use (e.g. `D:\tools\MyAppLauncher\config.txt`). It works like any other variable and can be redefined with `set config_file = ...`. For example, a launcher entry that opens the config file itself:

```
open_config_file;;notepad.exe;$config_file
```

Arguments are passed as written, so if the path may contain spaces and the program needs it quoted, write `"$config_file"`.

A line is skipped if it has fewer than 3 fields, an empty name, an unknown option, an exe that doesn't exist, or uses an undefined variable. A `set` line with an invalid variable name is skipped too. At startup each skipped line shows a dialog: *"config.txt line # X invalid, ignored"*, with the reason.

**Where the file is looked for:**

1. The first command-line argument, if given (`MyAppLauncher.exe D:\my\launcher.txt`)
2. `config.txt` next to `MyAppLauncher.exe`
3. `config.txt` in the current working directory

### 2. Use the launcher

```
+------------------------------------------------------------+
| [put                                                     ] |  <- User Input
+------------------------------------------------------------+
| > putty localhost (putty.exe -load "ubuntu32gb" -l "igor") ^|  <- Program List
|   putty server (once_exe putty.exe -load "server")         |
|                                                           v|
+------------------------------------------------------------+
```

Each entry is shown as `<name> (<options> <exe_file_without_path> <arguments_string>)`. The options and arguments are left out when empty, e.g. `calc (once_exe calc.exe)`. Only the name is used for matching what you type.

| Key / action | Effect |
|---|---|
| Typing | Filters the list (ignores upper/lower case). Exact name matches come first, then names that start with the typed text. The first entry is selected |
| **Up** / **Down** | Moves the selection |
| **Enter** | Starts the selected program, or switches to it if it's running and an option is set. The launcher then minimizes. If the program can't be started, an error message is shown and the launcher stays open |
| Double-click | Same as Enter for the clicked entry |
| **Esc** | Minimizes the launcher |

Hover over an entry to see the full command line it will run. When you bring the launcher back from the taskbar, the input text is selected so you can type a new name straight away.

### Window position and size

When the launcher is closed, its window position and size are saved to `MyAppLauncher.ini` next to `MyAppLauncher.exe`. They're applied the next time it starts. If the window would be off-screen (e.g. a monitor was disconnected), Qt moves it back onto a visible screen. Delete `MyAppLauncher.ini` to go back to the default size and position.

### Limitations

- **`once_exe_arg` needs Windows 8.1 or later,** because reading another process's command line uses a Windows function added in 8.1.
- **Switching needs a visible main window.** If the matching process has none (for example it lives only in the tray), a new instance is started.
- **Programs that start another program and exit can't be detected.** On Windows 11, `calc.exe` launches `CalculatorApp.exe` and quits.
- **Programs that reuse one instance may not match `once_exe_arg`.** Excel usually opens new files in its existing process, so the running Excel shows the command line it was first started with.

## Architecture

```
main.cpp
  ├─ ConfigParser ──► QList<ProgramEntry> ──► QFileIconProvider (icons)
  └─ LauncherWindow (QWidget)
        ├─ QSettings          window geometry in MyAppLauncher.ini (restored in constructor, saved in closeEvent)
        ├─ QLineEdit          User Input; key handling via eventFilter
        ├─ QListView          Program List
        │    └─ QStandardItemModel   rebuilt on every text change
        └─ ProcessUtils::launch()    on Enter / double-click → on success: minimize launcher
              ├─ switchToRunning()   Toolhelp32 snapshot → match exe/args → activate window
              └─ startProgram()      CreateProcessW
```

| File | Responsibility |
|---|---|
| `src/programentry.h` | `ProgramEntry` struct: name, option, exe (as configured and resolved), arguments, icon, line number |
| `src/configparser.h/.cpp` | Reads and checks `config.txt`, handles `set` variables (including the predefined `$config_file`) and `$name` replacement. Returns the valid entries plus a list of line errors |
| `src/processutils.h/.cpp` | All Windows-specific code (see below) |
| `src/launcherwindow.h/.cpp` | The UI: list text, filtering, selection, keys, launching, minimizing, saving/restoring window geometry |
| `src/main.cpp` | Finds the config file, shows the error dialogs, loads icons, shows the window |

### `ProcessUtils` in detail

- **Resolving exe paths:** `%VARIABLES%` are expanded with `ExpandEnvironmentStringsW`, and relative names are looked up with `SearchPathW`. The Qt 5.1.1 MSVC 2010 build is 32-bit, so on 64-bit Windows paths under `System32` are mapped to `Sysnative`. That makes the native 64-bit system programs start, and lets them be matched when they're running.
- **Starting a program:** `CreateProcessW` is called with the resolved exe and the built command line. `AllowSetForegroundWindow` lets the new program come to the front.
- **Finding a running instance:**
  1. `CreateToolhelp32Snapshot` lists all processes, filtered by exe file name.
  2. `QueryFullProcessImageNameW` gives each process's full exe path, compared ignoring case.
  3. For `once_exe_arg`, `NtQueryInformationProcess(ProcessCommandLineInformation)` reads the process's command line. This also works for 64-bit processes when the launcher is 32-bit. The command line is split with `CommandLineToArgvW`, and the configured arguments must appear in it in the same order (case-insensitive).
- **Switching:** `EnumWindows` finds the process's visible top-level window that has no owner. The window is restored if minimized, then `BringWindowToTop()` and `SetForegroundWindow()` are called.

APIs that aren't declared in older SDK headers are loaded at runtime with `GetProcAddress`. The code is C++03 because MSVC 2010 only partly supports C++11.

## How to build

### Requirements

- Qt 5.1.1 for MSVC 2010 (32-bit), e.g. installed at `C:\Qt\Qt5.1.1\5.1.1\msvc2010`
- Microsoft Visual Studio 2010 (or the Visual C++ 2010 build tools)

### Option A: build script

```bat
build.bat
```

The script sets up the VS2010 x86 environment, runs `qmake` and `nmake` in `build\`, and copies `config.txt` next to the exe. The result is `build\release\MyAppLauncher.exe`. If Qt or Visual Studio is installed somewhere else, change `QTDIR` and `VCVARS` at the top of the script.

### Option B: Qt Creator

Open `MyAppLauncher.pro`, select the *Desktop Qt 5.1.1 MSVC2010 32bit* kit, and build. Put `config.txt` next to the built exe, or pass its path as an argument.

### Option C: manually

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86
set PATH=C:\Qt\Qt5.1.1\5.1.1\msvc2010\bin;%PATH%
mkdir build && cd build
qmake ..\MyAppLauncher.pro -spec win32-msvc2010 "CONFIG+=release"
nmake
```

### Running and deploying

To run the exe outside Qt Creator, either put `C:\Qt\Qt5.1.1\5.1.1\msvc2010\bin` on `PATH`, or copy these files next to `MyAppLauncher.exe`:

```
Qt5Core.dll  Qt5Gui.dll  Qt5Widgets.dll
icudt51.dll  icuin51.dll  icuuc51.dll
libEGL.dll   libGLESv2.dll  D3DCompiler_43.dll
platforms\qwindows.dll        (from C:\Qt\Qt5.1.1\5.1.1\msvc2010\plugins\platforms)
```

On machines without Visual Studio 2010, also install the MSVC 2010 runtime (`C:\Qt\Qt5.1.1\vcredist\vcredist_sp1_x86.exe`).
