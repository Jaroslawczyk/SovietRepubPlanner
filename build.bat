@echo off
rem Builds build\dinput8.dll (x64). Run from any shell; needs Visual Studio Build Tools.
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist build\obj mkdir build\obj
set IMGUI=third_party\imgui
cl /nologo /O2 /MT /EHsc /utf-8 /std:c++17 /W3 /LD /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
  /I %IMGUI% /I %IMGUI%\backends /Fobuild\obj\ /Febuild\dinput8.dll ^
  src\main.cpp src\i18n.cpp src\engine.cpp src\catalog.cpp src\planner.cpp src\ui.cpp ^
  %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp ^
  %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_dx11.cpp ^
  /link /DEF:src\dinput8.def d3d11.lib dxgi.lib user32.lib d3dcompiler.lib ole32.lib windowscodecs.lib || exit /b 1
echo OK: build\dinput8.dll
