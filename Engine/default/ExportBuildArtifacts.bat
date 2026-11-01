@echo off
setlocal

set "ROOT=%~1"
set "CONFIG=%~2"
set "ENGINE_OUT=%~3"
set "SDK_LIB=%ROOT%EngineSDK\lib\%CONFIG%\"
set "CLIENT_OUT=%ROOT%Client\bin\%CONFIG%\"
set "EDITOR_OUT=%ROOT%Editor\bin\%CONFIG%\"

if "%ROOT%"=="" exit /b 1
if "%CONFIG%"=="" exit /b 1
if "%ENGINE_OUT%"=="" exit /b 1

echo [Engine Post-Build] Exporting %CONFIG% artifacts...
xcopy /Y /I "%ENGINE_OUT%Engine.lib" "%SDK_LIB%" || exit /b 1

xcopy /Y /I "%ROOT%Engine\public\*.h" "%ROOT%EngineSDK\inc\" || exit /b 1
xcopy /Y /I "%ROOT%Engine\private\*.inl" "%ROOT%EngineSDK\inc\" || exit /b 1
xcopy /Y /I "%ROOT%Engine\bin\shaders\*.hlsl*" "%ROOT%EngineSDK\inc\" || exit /b 1
xcopy /Y /I /E "%ROOT%Engine\external\FMOD" "%ROOT%EngineSDK\inc\" || exit /b 1
xcopy /Y /I /E "%ROOT%vcpkg_installed\x64-windows\include" "%ROOT%EngineSDK\inc\" || exit /b 1

xcopy /Y /I "%ROOT%Engine\bin\shaders\*.hlsl*" "%ENGINE_OUT%shaders\" || exit /b 1
xcopy /Y /I "%ENGINE_OUT%Engine.dll" "%CLIENT_OUT%" || exit /b 1
xcopy /Y /I "%ENGINE_OUT%Engine.dll" "%EDITOR_OUT%" || exit /b 1
xcopy /Y /I "%ENGINE_OUT%Engine.pdb" "%CLIENT_OUT%" || exit /b 1
xcopy /Y /I "%ENGINE_OUT%Engine.pdb" "%EDITOR_OUT%" || exit /b 1
xcopy /Y /I /E "%ENGINE_OUT%shaders" "%CLIENT_OUT%shaders\" || exit /b 1
xcopy /Y /I /E "%ENGINE_OUT%shaders" "%EDITOR_OUT%shaders\" || exit /b 1
xcopy /Y /I "%ROOT%Engine\external\FMOD\*.dll" "%CLIENT_OUT%" || exit /b 1
xcopy /Y /I "%ROOT%Engine\external\FMOD\*.dll" "%EDITOR_OUT%" || exit /b 1

echo [Engine Post-Build] %CONFIG% export complete.
exit /b 0
