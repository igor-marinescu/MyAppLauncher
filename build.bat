@echo off
rem Builds MyAppLauncher with Qt 5.1.1 (MSVC 2010, 32-bit) into .\build
setlocal
set QTDIR=C:\Qt\Qt5.1.1\5.1.1\msvc2010
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"

call %VCVARS% x86 || goto :error
set PATH=%QTDIR%\bin;%PATH%

if not exist build mkdir build
pushd build
qmake ..\MyAppLauncher.pro -spec win32-msvc2010 "CONFIG+=release" || goto :error_pop
nmake /nologo || goto :error_pop
copy /Y ..\config.txt release\config.txt >nul
popd

echo.
echo Build OK: build\release\MyAppLauncher.exe
echo To run outside Qt Creator, deploy the Qt DLLs, e.g.:
echo   copy %QTDIR%\bin\Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, icu*51.dll, libEGL.dll, libGLESv2.dll, D3DCompiler_43.dll
echo   and %QTDIR%\plugins\platforms\qwindows.dll into build\release\platforms\
exit /b 0

:error_pop
popd
:error
echo Build FAILED
exit /b 1
