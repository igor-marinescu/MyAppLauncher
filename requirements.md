# MyAppLauncher Requirements

"MyAppLauncher" is a Win32/64 application developed with Qt 5.1.1. It allows the user to select and start a program from a predefined list.

The user has a list of programs that they use regularly, some of which require specific parameters. The user needs an easy way to start the program they need. To do this, the user defines, in a text file, a list of all their programs together with the arguments (if required) for each program. Later, using MyAppLauncher, the user can start any of the predefined programs simply by typing the first characters of the program's name. MyAppLauncher looks up the program in the text file and launches it (with all required arguments).

There is also an option to check whether the program is already running; if it is, MyAppLauncher does not start it again, but simply switches to it.

## Configuration File

The user defines all programs that can be started by MyAppLauncher in a file called the "Configuration File" (`config.txt`).
The Configuration File is a text file with the following format:

- Each line defines either one program that can be started by MyAppLauncher, or a variable (see [Variables](#variables)).
- Empty lines are ignored.
- Lines that start with `#` are treated as comments and ignored.

A program definition in the Configuration File contains fields separated by semicolons (`;`):
```
<name>;<options>;<exe>;<argument1>;<argument2>;<argument3>;<argument4>;...
```

Where:
- `name` - the user-defined name of the program
- `options` - options for starting or switching to the program; optional, left empty if there are no options
- `exe` - the path and file name of the program (exe file)
- `argumentX` - the list of arguments used to execute the program

The following values are available for the `options` field:
- `once_exe` - check whether a program with this `exe` is already running; if so, switch to it (do not start it again)
- `once_exe_arg` - the same as `once_exe`, but additionally check whether the command line used to start the running application contains the same arguments; if so, switch to it

### Example 1 (no option)

The following line in the Configuration File:

```
putty localhost;;c:\Program Files\PuTTY\putty.exe;-load "ubuntu32gb";-l "igor"
```

defines a program named "putty localhost" with no options, which must be invoked using the following command line (built from the exe and arguments):

```
c:\Program Files\PuTTY\putty.exe -load "ubuntu32gb" -l "igor"
```

### Example 2 (`once_exe` option)

The following line in the Configuration File:

```
calc;once_exe;C:\Windows\System32\calc.exe
```

defines a program named "calc", which must be invoked using the following command line:

```
C:\Windows\System32\calc.exe
```

If the program is already running, it is not started again; MyAppLauncher just switches to it.

### Example 3 (`once_exe_arg` option)

The following line in the Configuration File:

```
excel hello;once_exe_arg;C:\Program Files (x86)\Microsoft Office\root\Office16\EXCEL.EXE;C:\hello.xls
```

defines a program named "excel hello", which must be invoked using the following command line (built from the exe and arguments):

```
C:\Program Files (x86)\Microsoft Office\root\Office16\EXCEL.EXE C:\hello.xls
```

If the program is already running with the same argument, it is not started again; MyAppLauncher just switches to it.

### Variables

To simplify editing and maintaining the Configuration File, a name, path or file name that occurs several times can be replaced with a variable.

A variable is defined on its own line:

```
set <variable_name> = <variable_value>
```

A variable is used by writing `$<variable_name>`. In every program definition after the variable's definition, each occurrence of `$<variable_name>` is replaced with the variable's value before the line is parsed.

Rules:
- The line starts with the keyword `set` followed by at least one space. White space around the name, around `=` and at the end of the value is ignored.
- `variable_name` consists of letters, digits and `_`, and must not start with a digit. Variable names are case-insensitive (`$Project_Path` and `$project_path` are the same variable).
- `variable_value` is everything after `=` (trimmed). It may be empty, may contain `;` (e.g. to hold several arguments), and may use variables defined earlier, which are replaced when the variable is defined.
- A variable can be used only after it has been defined. Defining a variable again replaces its value for all following lines.
- In `$<variable_name>`, the name is the longest sequence of letters, digits and `_` following `$`. For example, in `$project_path\file.txt` the variable is `project_path`.
- `$$` is replaced with a literal `$`. A `$` that is not followed by a letter, `_` or `$` is kept as is (e.g. `\\server\c$\dir`).
- Variables are replaced in all fields (name, options, exe and arguments).
- If a variable definition is invalid (e.g. invalid variable name), or a program definition uses an undefined variable, the line is ignored and the dialog box "config.txt line # X invalid, ignored" is displayed.

### Example 4 (variables)

The following lines in the Configuration File:

```
set project_path = C:\project\path
open_file;;notepad.exe;$project_path\file.txt
```

are equivalent to:

```
open_file;;notepad.exe;C:\project\path\file.txt
```

## UI

The User Interface (UI) consists of two elements:

```
+----------------------------+---
| [program                 ] | User Input
+----------------------------+---
| - program x               ^| 
| - program y                | Program List
| - program z                |
|                           v|
+----------------------------+---
```

- User Input (top) - a line edit (`QLineEdit`) where the user enters the name of the program they want to start (or switch to).
- Program List (center) - a list (`QListView`) of the programs from the Configuration File whose names fully or partially match the text entered in the User Input field. Each element in the Program List is displayed with the program's icon (extracted from the exe file) and the following text:

```
<name> (<options> <exe_file_without_path> <arguments_string>)
```

Where:
- `name` - the program name
- `options` - the `options` field (omitted if empty)
- `exe_file_without_path` - the file name of the `exe` field, without the path (e.g. `putty.exe`)
- `arguments_string` - all `argumentX` fields joined by spaces (omitted if there are no arguments)

For example, the programs from Examples 1-3 are displayed as:

```
putty localhost (putty.exe -load "ubuntu32gb" -l "igor")
calc (once_exe calc.exe)
excel hello (once_exe_arg EXCEL.EXE C:\hello.xls)
```

Only the `name` part is used when matching the text entered in the User Input field.

## Functionality

- When MyAppLauncher starts, it opens the Configuration File (`config.txt`), parses all lines, and loads the program information into memory.
    - If a line in the Configuration File contains errors (for example, the program's exe file is invalid), the line is ignored. In this case, a dialog box is displayed: "config.txt line # X invalid, ignored".
    - For each program listed in the Configuration File, its icon (`QIcon`) is extracted from the corresponding exe file (using `QFileIconProvider`) and also stored in memory.
- The MyAppLauncher window is displayed:
    - The window position and size saved when MyAppLauncher was last closed are restored and applied. If none were saved (e.g. on the first start), a default position and size are used.
    - The User Input field is empty.
    - The Program List displays the names (and icons) of all programs loaded from the Configuration File.
    - If the Program List is not empty, the first program is selected.
    - The User Input field has the input focus, so the user can begin typing the name of the program they wish to launch (or switch to).
- As soon as the user begins typing in the User Input field, MyAppLauncher checks, with every character entered, whether any program names match the input text, either fully or partially (i.e., the input text matches the beginning of a program name). The matching programs are displayed in the Program List: programs whose names exactly match the input text appear first, followed by programs whose names only begin with the input text. The first entry in the Program List is selected.
- When the user presses the "Down" or "Up" arrow key (and the Program List is not empty), the selection in the Program List moves down or up accordingly.
- When the user presses the "Enter" key (and the Program List is not empty):
    - The record corresponding to the program selected in the Program List is built using data from the Configuration File.
    - The command line for that record is built from the `exe` and `argumentX` fields.
    - If the `options` field is empty, the command line is executed and the program is started.
    - If the `options` field is `once_exe`, MyAppLauncher builds a list of all running programs:
        - If a running program matches the `exe` field (i.e., has the same executable file name), MyAppLauncher switches to it (`BringWindowToTop()`, `SetForegroundWindow()`).
        - If no running program matches the `exe` field, the command line is executed and the program is started.
    - If the `options` field is `once_exe_arg`, MyAppLauncher builds a list of all running programs (including the command line used to start each program):
        - If a running program matches the command line (i.e., has the same executable file name and arguments), MyAppLauncher switches to it (`BringWindowToTop()`, `SetForegroundWindow()`).
        - If no running program matches the command line, the command line is executed and the program is started.
    - When the program has been started or switched to, the MyAppLauncher window is sent to the background and minimized. If the program cannot be started, an error message is displayed and the MyAppLauncher window stays open.
- When the user presses the "Esc" key, the MyAppLauncher window is sent to the background and minimized.
- When MyAppLauncher is closed, the position and size of its window are saved, so they can be restored the next time MyAppLauncher is started.
