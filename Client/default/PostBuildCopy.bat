@echo off
setlocal

set "ROOT=%~1"
set "CONFIG=%~2"
set "CLIENT_DLL=%~3"
set "CLIENT_OUT=%ROOT%Client\bin\%CONFIG%\"
set "EDITOR_OUT=%ROOT%Editor\bin\%CONFIG%\"
set "LAUNCHER_OUT=%ROOT%Launcher\bin\%CONFIG%\"

if "%ROOT%"=="" exit /b 1
if "%CONFIG%"=="" exit /b 1
if not exist "%CLIENT_DLL%" exit /b 1
if not exist "%CLIENT_OUT%Client.lib" exit /b 1
if not exist "%CLIENT_OUT%Client.pdb" exit /b 1

echo [Client Post-Build] Copying %CONFIG% artifacts...
xcopy /Y /I "%ROOT%Client\bin\shaders\*.hlsl*" "%CLIENT_OUT%shaders\" || exit /b 1
xcopy /Y /I "%CLIENT_DLL%" "%EDITOR_OUT%" || exit /b 1
xcopy /Y /I "%CLIENT_OUT%Client.pdb" "%EDITOR_OUT%" || exit /b 1
xcopy /Y /I /E "%CLIENT_OUT%shaders" "%EDITOR_OUT%shaders\" || exit /b 1
xcopy /Y /I "%CLIENT_DLL%" "%LAUNCHER_OUT%" || exit /b 1
xcopy /Y /I "%CLIENT_OUT%Client.pdb" "%LAUNCHER_OUT%" || exit /b 1
xcopy /Y /I /E "%CLIENT_OUT%shaders" "%LAUNCHER_OUT%shaders\" || exit /b 1

echo [Client Post-Build] %CONFIG% copy complete.
exit /b 0
